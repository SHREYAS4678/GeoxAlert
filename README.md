# GeoAlert — deployment bundle

This bundle organizes the **firmware and dashboard you supplied** into a clean PlatformIO + Streamlit project. It removes hard-coded credentials, cleans the PlatformIO configuration, adds a local-only configuration wizard, removes the unsupported siren controls, and includes an optional Twilio queue-worker scaffold.

> **Security first:** credentials and a Firebase service-account private key were pasted/shared during troubleshooting. Treat them as compromised. Before using this bundle, change the Firebase Authentication password, change the Wi-Fi password, revoke/delete the exposed service-account key in Google Cloud IAM, and restrict the Firebase Web API key. Generate a new service-account key only if you need the optional Twilio worker. Do not reuse the old key.

## What is included

- `firmware/`: ESP32 PlatformIO code for soil moisture, FC-37 sensor index, DHT22, SW-420, MPU-compatible 6-axis IMU, Firebase telemetry, Wi-Fi/GPRS failover and LittleFS spool.
- `dashboard/app.py`: your GeoAlert Command Hub dashboard (Streamlit) with Firebase REST access, risk scoring, infinite-slope Factor of Safety, forecast/anomaly views, history and exports.
- `backend/twilio_worker.py`: optional SMS queue-worker scaffold. It sends only to the explicit `to` number entered in the dashboard; the UI is single-recipient per queue item. No SMS is sent until the worker is configured and a user explicitly queues a message.
- `scripts/`: PowerShell setup, config, build, upload, serial monitor, dashboard, worker and GitHub publication helpers.
- `*.example`: safe configuration templates with placeholders only.

## Important scope / honesty

The supplied dashboard computes a weighted threat score and an infinite-slope FoS model. The source provided in this request did **not** include a training dataset, trained Random Forest/XGBoost model files, or an existing prediction API, so this bundle does not claim those models are integrated. Use locally validated calibration/data before relying on any risk tier for field decisions.

The firmware uses SIM800L for GPRS telemetry failover. This bundle also adds an **opt-in, threshold-based direct SIM800L SMS helper**, disabled by default; it requires testing with your particular SIM800L V2 board, carrier, power supply, and phone number. The prototype has **no physical siren or relay**. The dashboard queues cloud SMS requests but message delivery requires an explicitly configured worker and recipient mapping.

The FC-37 output is a sensor index/percentage, not calibrated rainfall in millimetres. The dashboard treats it as `rainfall_pct`; do not pass it to any model that expects mm unless you calibrate it.

## 1. Install prerequisites

- Windows 10/11
- Python 3.11 (recommended)
- Git for Windows
- A data USB cable for your ESP32
- PlatformIO is installed by the setup script into the project's `.venv`.

Open PowerShell in the extracted `GeoAlert_Deployment` folder:

```powershell
Set-ExecutionPolicy -Scope Process Bypass
.\scripts\setup.ps1
```

## 2. Configure your local secrets

After revoking the exposed key and changing passwords, run:

```powershell
.\scripts\configure.ps1
```

Enter your new Wi-Fi/Firebase credentials. For phone number, use E.164 format like `+91...` (replace with your real destination). Leave blank and keep SMS disabled until the modem/network has been tested. Type exactly `YES` to enable direct SIM800L SMS; any other input keeps it disabled.

If you choose to configure the optional Twilio worker, provide a **new** Firebase Admin SDK service-account JSON file created after the old key is revoked. The wizard validates it and copies it to `private/firebase-service-account.json`, which `.gitignore` excludes. The private-key file is never part of this ZIP.

## 3. Build/upload ESP32 firmware

```powershell
.\scripts\build-firmware.ps1
.\scripts\upload-firmware.ps1
.\scripts\monitor-serial.ps1
```

The upload script asks for a COM port; type something such as `COM3` if auto-detection is not available. The serial monitor uses 115200 baud. Close the serial monitor before uploading again.

### Pin mapping

| Component | ESP32 pin |
|---|---:|
| Soil moisture analog output | GPIO 34 |
| FC-37 analog output | GPIO 35 |
| DHT22 data | GPIO 4 |
| SW-420 digital output | GPIO 5 |
| IMU SDA / SCL | GPIO 21 / GPIO 22 |
| SIM800L TXD → ESP32 RX | GPIO 16 |
| ESP32 TX → SIM800L RXD | GPIO 17 |

Power the SIM800L V2 board according to that board's input specification; a modem can brown out during current bursts. Share ground with the ESP32, connect the antenna, and verify cellular registration. The code uses a prototype local SMS trigger (tilt >= 25°, or soil/rain-index/vibration combinations) with a 15-minute cooldown. Do not enable it until the actual sensor calibration and network have been tested safely.

## 4. Run dashboard

```powershell
.\scripts\run-dashboard.ps1
```

Open <http://localhost:8501>. Leave that terminal open. If no Firebase credentials are configured or database rules reject access, the dashboard may show **DEMO / RECONNECTING** data; that is not live telemetry.

The ESP32 writes live data to `/nodes/geoalert-node-01/live` and historical readings to `/nodes/geoalert-node-01/history`. Change the node ID in the firmware config if you use another one; keep it consistent with the dashboard node selector.

## 5. Optional Twilio worker

The worker needs `TWILIO_ACCOUNT_SID`, `TWILIO_AUTH_TOKEN`, `TWILIO_FROM_NUMBER`, a valid new `private/firebase-service-account.json`, and correct Firebase database URL in local `.env`.

```powershell
.\scripts\run-alert-worker.ps1
```

**Before using this worker for real alerts:** the dashboard now requires one explicit recipient number per queued message. It does not resolve a zone into a resident list or send bulk messages. Validate opt-in, local legal requirements, delivery receipts, failure handling and Twilio test costs.

## 6. Publish to GitHub

Create a **new empty repository** on GitHub (do not initialize it with README/license), then run:

```powershell
.\scripts\publish-github.ps1
```

The script initializes Git, checks staged filenames for common secrets, commits, asks for the repository URL, and pushes the `main` branch. Sign in using Git Credential Manager or SSH. Do not paste a GitHub token into chat or source files. Review `git status` before publication. Never force-add ignored secret files.

## Troubleshooting

- Port 8000 is not used by this bundle; the supplied app is Streamlit-only and listens on port 8501. If an older GeoAlert backend is running on 8000, it is a separate project process.
- `Extra data: line ... column ...` from JSON parsing means a supposed JSON file includes extra text. The service-account JSON must contain only one JSON object; never append `#define` firmware config to it.
- Firebase login failures can mean stale credentials, disabled Email/Password provider, too many attempts, incorrect DB URL, or database rules. Wait out auth rate limits and verify settings in the Firebase Console.
- If the sensor/dashboard values are wrong, validate calibration: FC-37 analog percentage is not rainfall mm; roll/pitch based tilt should be calibrated relative to the mounted board.
- If direct SMS fails: check stable modem power, SIM PIN/status, antenna, carrier 2G support, `AT+CSQ`/`AT+CREG?`, and SIM800L current bursts. Many modern networks do not offer 2G everywhere.

## Safety note

This is an experimental decision-support prototype, not a certified landslide warning system. Do not use outputs as the sole basis for evacuation or public-safety decisions. Validate sensors, geotechnical parameters, thresholds, and notification delivery with qualified personnel and local authorities before any field deployment.
