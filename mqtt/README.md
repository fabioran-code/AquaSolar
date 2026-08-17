# AquaSolar MQTT

Architecture:

ESP32 -> MQTT broker -> AquaSolar dashboard

Topics:
- `aquasolar/<deviceId>/telemetry` published by ESP32
- `aquasolar/<deviceId>/status` published by ESP32
- `aquasolar/<deviceId>/command` subscribed by ESP32

Telemetry JSON example:
```json
{"ph":7.21,"temperature":25.4,"humidity":61,"waterLevel":78,"pump":false,"status":"NORMAL"}
```

For browser clients, use MQTT over secure WebSockets (`wss://`). Do not put broker passwords in frontend source code. Use a broker with TLS, authentication and ACLs.

Simulation mode should use the same telemetry schema so the dashboard can switch between simulated and real sources without changing its widgets.
