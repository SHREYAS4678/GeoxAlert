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

The firmware publishes records under `/nodes/<node_id>/live` and `/nodes/<node_id>/history`. Dashboard alerts are queued under `/alerts/queue`. The existing dashboard's risk calculation is a weighted hazard score combined with an infinite-slope factor-of-safety heuristic. A validated supervised ML model pipeline is not supplied in this bundle because its dataset and trained model artifacts were not attached with the requested source code.
