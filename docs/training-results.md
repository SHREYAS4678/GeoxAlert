# GeoAlert ML training results

Training was completed using the provided `geoalert_training_dataset.csv` file (9,864 rows, no missing data in the selected columns). Eight predictors were used: `soil_moisture`, `rainfall`, `temperature`, `humidity`, `roll`, `pitch`, `yaw`, and `vibration`. Target: numeric binary `label`.

The reported evaluation uses a stratified 80/20 holdout split (1,973 test rows, random seed 42).

| Model | Accuracy | Macro F1 | ROC AUC |
|---|---:|---:|---:|
| Random Forest | 99.70% | 99.70% | 0.9997 |
| XGBoost | 99.65% | 99.65% | 0.9998 |
| Soft-voting ensemble | 99.65% | 99.65% | 0.9997 |

Random Forest confusion matrix with classes `[0, 1]`: `[[979, 2], [4, 988]]`.

## Limitations that must be resolved

- The source CSV does not define whether class `0` or `1` means Normal, landslide, or risk. The model is a binary classifier, not a four-tier risk classifier.
- The source CSV does not document the unit of its `rainfall` column. The hardware FC-37 emits an analog wetness/index reading, not calibrated rainfall in millimetres.
- Very high holdout scores can occur if simulated labels follow deterministic rules or if the dataset has leakage; the metrics are not evidence of field reliability.
- The current dashboard continues to show its weighted heuristic/FoS scores; it must not display these binary outputs as operational hazard tiers until the label meanings and units are confirmed and the live feature adapter is validated.

The training code is in `train_models.py`. The local training CSV and binary artifacts are excluded from the Git repository by `.gitignore`; the complete deployment bundle contains the trained artifacts, CSV and reports.
