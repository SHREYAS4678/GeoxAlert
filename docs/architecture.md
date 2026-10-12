# GeoAlert architecture and current implementation

```mermaid
flowchart LR
    A["Sensor layer<br/>Capacitive soil moisture<br/>FC-37 rain index<br/>DHT22 · SW-420 · MPU6050"] --> B["ESP32 edge node<br/>Sampling + JSON telemetry"]
    B --> C["Network manager<br/>Wi-Fi primary<br/>SIM800L GPRS fallback"]
    B <--> D["LittleFS offline spool<br/>Store during outage<br/>Replay after reconnect"]
    C --> E[("Firebase Realtime Database<br/>nodes/<node_id>/live<br/>nodes/<node_id>/history")]
    E --> F["Streamlit Command Hub"]
    F --> G["Visualization / analytics<br/>Risk score · FoS<br/>Forecast · anomaly checks<br/>Node health · report exports"]
    B -. "optional direct SMS<br/>disabled by default" .-> H["SIM800L SMS"]
```

## Data path

1. Sensors are sampled by the ESP32 and assembled into a JSON telemetry object.
2. Wi-Fi is the preferred uplink. SIM800L GPRS is the fallback. If both uplinks fail, telemetry is written to LittleFS and replayed after reconnection.
3. Live data is written to `/nodes/<node_id>/live`; historical records are written to `/nodes/<node_id>/history`.
4. The Streamlit dashboard reads Firebase and presents live sensor values, node health, weighted hazard score, infinite-slope Factor of Safety, history, forecasts, anomaly indicators, map/fleet views and downloadable reports.
5. Webhook escalation notifications are optional and only work when a webhook URL is configured. Direct SIM800L SMS is an optional firmware-side feature and is disabled by default.

The repository does not include a cloud SMS worker or a dashboard SMS queue. No physical siren or relay is installed.

## Live-versus-demo behavior

The dashboard displays `LIVE STREAMING` only when a Firebase live record is returned for the selected node. If the node path is empty or inaccessible, inspect the serial monitor, Firebase Authentication, Realtime Database rules, database URL and firmware's configured node ID. Historical graphs may use synthetic fallback data when no device history is available.

## Current analytical limitations

The displayed hazard tier is based on a weighted heuristic and an infinite-slope FoS calculation. The separate training pipeline's supplied binary model is not wired into the live hazard tier. Its metrics should not be interpreted as validated field performance.
