# GeoAlert — AI/IoT landslide-monitoring prototype

GeoAlert combines ESP32 telemetry, Firebase Realtime Database, a Streamlit Command Hub, a weighted sensor-risk score/infinite-slope Factor of Safety heuristic, and optional GSM/cloud notifications.

> **Security first:** Wi-Fi and Firebase credentials plus a Firebase service-account private key were shared during troubleshooting. Treat them as compromised: change the Wi-Fi and Firebase Authentication passwords, revoke/delete the exposed service-account key in Google Cloud IAM, and restrict the Firebase Web API key. Generate a replacement Admin SDK key only if you need the optional Twilio worker. Never commit `.env`, `secrets.h`, Streamlit secrets or service-account JSON.

## Repository contents

- `firmware/src/main.cpp` and `firmware/platformio.ini`: ESP32 Arduino/PlatformIO firmware using the soil sensor, FC-37, DHT22, SW-420, MPU-compatible IMU, Firebase REST, Wi-Fi/GPRS failover and LittleFS spool.
- `dashboard/app.py`: Streamlit Command Hub with Firebase REST access, weighted hazard score, infinite-slope FoS, forecast/anomaly views, history and exports. It no longer suggests that the prototype has a physical siren.
- `backend/twilio_worker.py`: optional Twilio SMS queue worker; requires new credentials, service-account JSON and an explicit destination number per queued item.
- `train_models.py`: reproducible Random Forest, XGBoost and soft-voting training pipeline.
- `scripts/`: PowerShell configuration, setup, training, firmware build/upload/monitor, dashboard and optional alert-worker launchers.
- `docs/`: architecture, Firebase configuration and ML evaluation caveats.

The repository intentionally excludes local credentials, model binaries and CSV files via `.gitignore`. The provided **complete deployment ZIP** includes the 9,864-row training CSV and trained model artifacts; extract it to train/use the supplied artifacts, or copy your dataset into `data/geoalert_training_dataset.csv`.

## Quick start (Windows PowerShell)

```powershell
Set-ExecutionPolicy -Scope Process Bypass
.\scripts\setup.ps1
.\scripts\configure.ps1
```

After rotating the exposed credentials, complete the local prompts. Keep `geoalert-node-01` as the Node ID unless intentionally changing it. Do not enable direct GSM SMS until the exact SIM800L board power, SIM readiness, antenna and network registration have been checked.

## Train the supplied data/models

The included training dataset has these predictors:

`soil_moisture, rainfall, temperature, humidity, roll, pitch, yaw, vibration`

Target: numeric `label` (classes 0 and 1). To retrain on Windows, make sure `data/geoalert_training_dataset.csv` exists, then run:

```powershell
.\scripts\train-models.ps1
```

The script writes `models/random_forest_model.pkl`, `models/xgboost_model.pkl`, `models/voting_ensemble_model.pkl`, `models/scaler.pkl`, `models/feature_order.json`, and reports under `reports/`.

### Holdout metrics for the supplied CSV

A stratified 80/20 holdout with seed 42 yielded:

| Model | Accuracy | Macro F1 | ROC-AUC |
|---|---:|---:|---:|
| Random Forest | 99.70% | 99.70% | 0.9997 |
| XGBoost | 99.65% | 99.65% | 0.9998 |
| Soft-voting ensemble | 99.65% | 99.65% | 0.9997 |

See [training results and limitations](docs/training-results.md). These unusually strong metrics apply to this dataset only and do not prove real-world landslide performance.

### Critical ML limitations

- The dataset does not define whether label 0 or 1 means normal, risk or landslide. The model is binary; it does not produce four hazard tiers.
- The rainfall unit is not documented. The hardware FC-37 provides a wetness/index reading, not calibrated rainfall in millimetres. Do not assume they match.
- Firmware sensor units, IMU angle conventions and vibration aggregation must match the dataset before live model inference.
- The current Streamlit dashboard still uses the weighted heuristic and Factor of Safety. The trained artifacts are not yet wired into the dashboard's live hazard tier; do not relabel model class 0/1 as operational tiers until semantics and units are confirmed.

## Build and upload firmware

```powershell
.\scripts\build-firmware.ps1
.\scripts\upload-firmware.ps1
.\scripts\monitor-serial.ps1
```

The uploader asks for a COM port. Close the serial monitor before uploading. Pin map: soil GPIO 34, FC-37 GPIO 35, DHT22 GPIO 4, SW-420 GPIO 5, I2C SDA 21/SCL 22, SIM800L TXD to ESP32 RX GPIO 16 and ESP32 TX GPIO 17 to SIM800L RXD. Power SIM800L according to the exact V2 board specification with sufficient transient current and a shared ground.

## Run dashboard

```powershell
.\scripts\run-dashboard.ps1
```

Open http://localhost:8501. This version is a Streamlit-only dashboard and does not need the previous port-8000 backend. Without correct Firebase access, readings may be demo/offline values rather than live sensor data.

## Optional Twilio cloud SMS

Configure new Twilio credentials and a newly generated Firebase Admin service account locally, then run:

```powershell
.\scripts\run-alert-worker.ps1
```

Each queued message needs an explicit recipient phone number. Queueing is not proof of delivery; check Twilio and worker status. Charges may apply. Direct SIM800L SMS is an opt-in prototype. No physical siren or relay is installed.

## Safety

GeoAlert is an experimental decision-support prototype, not a certified landslide-warning system. Do not rely on it as the sole basis for evacuation or other public-safety decisions. Validate sensors, soil/geotechnical parameters, thresholds, class semantics, and notification delivery with qualified personnel and local authorities.
