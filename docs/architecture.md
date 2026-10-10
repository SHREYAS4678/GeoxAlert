# GeoAlert architecture and current implementation

```text
Soil / FC-37 / DHT22 / SW-420 / IMU
                 |
                ESP32
                 |  (Wi-Fi primary, SIM800L GPRS fallback, LittleFS spool)
                 v
       Firebase Realtime Database
          |                   |
          v                   v
  Streamlit Command Hub   Optional Twilio Worker
  (risk score, FoS,        (requires recipient mapping
   forecasts, health,       and credentials; not active
   incident queue)          by default)

Optional local direct SIM800L SMS: firmware helper, disabled by default.
No physical siren or siren relay is installed.
```

The firmware publishes records under `/nodes/<node_id>/live` and `/nodes/<node_id>/history`. Dashboard alerts are queued under `/alerts/queue`. The dashboard's current risk calculation is a weighted hazard score combined with an infinite-slope factor-of-safety heuristic. A validated supervised ML pipeline is not included because no training dataset or trained model artifacts were supplied with the source code.
