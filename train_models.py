"""Train GeoAlert Random Forest, XGBoost, and soft-voting models.

Requires data/geoalert_training_dataset.csv with numeric columns:
soil_moisture, rainfall, temperature, humidity, roll, pitch, yaw, vibration, label.
The target's 0/1 meaning and the rainfall unit must be verified before deployment.
"""
from __future__ import annotations
import hashlib, json, platform
from datetime import datetime, timezone
from pathlib import Path

import joblib
import matplotlib
matplotlib.use("Agg")
import matplotlib.pyplot as plt
import numpy as np
import pandas as pd
import sklearn
import xgboost
from sklearn.base import clone
from sklearn.ensemble import RandomForestClassifier, VotingClassifier
from sklearn.metrics import accuracy_score, precision_score, recall_score, f1_score, roc_auc_score, confusion_matrix, classification_report
from sklearn.model_selection import train_test_split
from sklearn.pipeline import Pipeline
from sklearn.preprocessing import StandardScaler
from xgboost import XGBClassifier

ROOT = Path(__file__).resolve().parent
CSV = ROOT / "data" / "geoalert_training_dataset.csv"
MODELS = ROOT / "models"
REPORTS = ROOT / "reports"
FEATURES = ["soil_moisture", "rainfall", "temperature", "humidity", "roll", "pitch", "yaw", "vibration"]
TARGET = "label"
SEED = 42

def digest(path: Path) -> str:
    h = hashlib.sha256()
    with path.open("rb") as f:
        for chunk in iter(lambda: f.read(1024 * 1024), b""):
            h.update(chunk)
    return h.hexdigest()

def make_pipe(estimator):
    return Pipeline([("scaler", StandardScaler()), ("classifier", estimator)])

def metric_row(name, model, xt, yt):
    pred, prob = model.predict(xt), model.predict_proba(xt)
    classes = list(model.classes_)
    result = {
        "model": name,
        "accuracy": float(accuracy_score(yt, pred)),
        "precision_macro": float(precision_score(yt, pred, average="macro", zero_division=0)),
        "recall_macro": float(recall_score(yt, pred, average="macro", zero_division=0)),
        "f1_macro": float(f1_score(yt, pred, average="macro", zero_division=0)),
        "confusion_matrix": confusion_matrix(yt, pred, labels=classes).tolist(),
        "classification_report": classification_report(
            yt, pred, labels=classes, target_names=[str(c) for c in classes],
            output_dict=True, zero_division=0
        ),
        "roc_auc": None
    }
    if len(classes) == 2:
        try:
            result["roc_auc"] = float(roc_auc_score(yt, prob[:, classes.index(1) if 1 in classes else 1]))
        except ValueError:
            pass
    return result

def main():
    if not CSV.is_file():
        raise FileNotFoundError(f"Dataset not found: {CSV}. Put geoalert_training_dataset.csv in data/.")
    df = pd.read_csv(CSV)
    missing = [c for c in FEATURES + [TARGET] if c not in df.columns]
    if missing:
        raise ValueError(f"Missing columns {missing}; available columns: {df.columns.tolist()}")
    X = df[FEATURES].apply(pd.to_numeric, errors="coerce")
    y0 = df[TARGET]
    valid = X.notna().all(axis=1) & y0.notna()
    removed = int((~valid).sum())
    X, y = X.loc[valid].reset_index(drop=True), y0.loc[valid].reset_index(drop=True)
    if len(X) < 50 or y.nunique() < 2:
        raise ValueError(f"Not enough usable rows/classes: rows={len(X)}, classes={y.nunique()}")
    if not pd.api.types.is_numeric_dtype(y):
        raise ValueError("The label column must contain numeric class labels for this script.")
    y = y.astype(int)
    counts = y.value_counts().sort_index()
    if counts.min() < 2:
        raise ValueError(f"Each class must have at least two rows: {counts.to_dict()}")

    Xtr, Xte, ytr, yte = train_test_split(X, y, test_size=0.20, random_state=SEED, stratify=y)
    rf = RandomForestClassifier(
        n_estimators=300, max_depth=16, min_samples_leaf=2,
        class_weight="balanced_subsample", random_state=SEED, n_jobs=-1
    )
    xgb = XGBClassifier(
        n_estimators=300, max_depth=5, learning_rate=0.05, subsample=0.9,
        colsample_bytree=0.9, reg_lambda=1.0,
        objective="binary:logistic" if y.nunique() == 2 else "multi:softprob",
        eval_metric="logloss" if y.nunique() == 2 else "mlogloss",
        tree_method="hist", random_state=SEED, n_jobs=4,
    )
    if y.nunique() > 2:
        xgb.set_params(num_class=int(y.nunique()))
    ensemble = VotingClassifier(estimators=[("random_forest", clone(rf)), ("xgboost", clone(xgb))], voting="soft")
    evaluations = []
    for name, est in [("Random Forest", rf), ("XGBoost", xgb), ("Soft-voting ensemble", ensemble)]:
        pipe = make_pipe(clone(est)).fit(Xtr, ytr)
        row = metric_row(name, pipe, Xte, yte)
        evaluations.append(row)
        print(f"{name}: accuracy={row['accuracy']:.4f}; macro-F1={row['f1_macro']:.4f}; ROC-AUC={row['roc_auc']}")

    # Refit deployable pipelines on all rows only after the holdout evaluation.
    MODELS.mkdir(parents=True, exist_ok=True)
    REPORTS.mkdir(parents=True, exist_ok=True)
    rf_final = make_pipe(clone(rf)).fit(X, y)
    xgb_final = make_pipe(clone(xgb)).fit(X, y)
    ensemble_final = make_pipe(clone(ensemble)).fit(X, y)
    joblib.dump(rf_final, MODELS / "random_forest_model.pkl", compress=3)
    joblib.dump(xgb_final, MODELS / "xgboost_model.pkl", compress=3)
    joblib.dump(ensemble_final, MODELS / "voting_ensemble_model.pkl", compress=3)
    joblib.dump(StandardScaler().fit(X), MODELS / "scaler.pkl", compress=3)

    metadata = {
        "target": TARGET,
        "target_classes": sorted(int(v) for v in y.unique()),
        "feature_order": FEATURES,
        "runtime_field_map": {
            "soil_moisture": "soil_moisture_pct",
            "rainfall": "rainfall_pct (unit mismatch until calibrated)",
            "temperature": "temperature_c", "humidity": "humidity_pct",
            "roll": "roll_deg", "pitch": "pitch_deg", "yaw": "yaw_deg",
            "vibration": "vibration_pulses"
        },
        "warning": "Confirm the meaning of labels 0/1. Dataset rainfall units are undocumented; FC-37 index/percentage is not automatically equivalent."
    }
    (MODELS / "feature_order.json").write_text(json.dumps(metadata, indent=2) + "\n", encoding="utf-8")
    imp = pd.DataFrame({
        "feature": FEATURES,
        "random_forest_importance": rf_final.named_steps["classifier"].feature_importances_
    }).sort_values("random_forest_importance", ascending=False)
    imp.to_csv(REPORTS / "random_forest_feature_importance.csv", index=False)
    fig, ax = plt.subplots(figsize=(9, 5))
    view = imp.sort_values("random_forest_importance")
    ax.barh(view["feature"], view["random_forest_importance"])
    ax.set_title("GeoAlert Random Forest feature importance")
    ax.set_xlabel("Relative impurity importance (not causal)")
    fig.tight_layout()
    fig.savefig(REPORTS / "feature_importance.png", dpi=150)
    plt.close(fig)

    report = {
        "project": "GeoAlert", "trained_at_utc": datetime.now(timezone.utc).isoformat(),
        "dataset": CSV.name, "dataset_sha256": digest(CSV),
        "rows_total": int(len(df)), "rows_used": int(len(X)), "rows_removed": removed,
        "features": FEATURES, "target": TARGET,
        "class_counts": {str(k): int(v) for k, v in counts.items()},
        "evaluation": {"method": "stratified holdout", "test_size": 0.20, "random_state": SEED,
                       "train_rows": int(len(Xtr)), "test_rows": int(len(Xte)), "models": evaluations},
        "final_artifacts_fit_on_rows": int(len(X)),
        "environment": {"python": platform.python_version(), "pandas": pd.__version__,
                        "numpy": np.__version__, "scikit_learn": sklearn.__version__, "xgboost": xgboost.__version__},
        "limitations": [
            "Binary labels 0/1 are preserved; semantics were not defined in the CSV.",
            "Rainfall units are undocumented; FC-37 percent/index is not automatically equivalent.",
            "Holdout metrics do not prove real-world landslide prediction reliability.",
            "This model predicts two classes, not four hazard tiers."
        ]
    }
    (REPORTS / "training_report.json").write_text(json.dumps(report, indent=2) + "\n", encoding="utf-8")
    lines = ["GeoAlert model training report", "===============================", f"Rows used: {len(X)} / {len(df)}",
             f"Class counts: {counts.to_dict()}", f"Features: {', '.join(FEATURES)}",
             f"Holdout: {len(Xtr)} train / {len(Xte)} test, stratified, random_state={SEED}", ""]
    for m in evaluations:
        lines += [m["model"], f"  Accuracy: {m['accuracy']:.4f}",
                  f"  Macro precision/recall/F1: {m['precision_macro']:.4f} / {m['recall_macro']:.4f} / {m['f1_macro']:.4f}",
                  f"  ROC AUC: {m['roc_auc']}", f"  Confusion matrix [classes {sorted(int(v) for v in y.unique())}]: {m['confusion_matrix']}", ""]
    lines += ["CAUTION: confirm meanings of labels 0/1. Rainfall units are undocumented and must be calibrated.",
              "Metrics are offline holdout results, not field validation. Do not use as the sole basis for public-safety decisions."]
    (REPORTS / "training_report.txt").write_text("\n".join(lines) + "\n", encoding="utf-8")
    print(f"Training complete. Artifacts are in {MODELS} and {REPORTS}.")
    print("Note: labels 0/1 and rainfall units require confirmation before live inference.")

if __name__ == "__main__":
    main()
