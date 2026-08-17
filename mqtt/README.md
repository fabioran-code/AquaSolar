# AquaSolar + HiveMQ Cloud

## Browser / Render
- Host: `dd3778f4cb2143faba675c1b1bc30546.s1.eu.hivemq.cloud`
- Secure WebSocket port: `8884`
- Path: `/mqtt`
- URL: `wss://dd3778f4cb2143faba675c1b1bc30546.s1.eu.hivemq.cloud:8884/mqtt`
- Username: `AquaSolar`

## ESP32
- Host: `dd3778f4cb2143faba675c1b1bc30546.s1.eu.hivemq.cloud`
- MQTT over TLS port: `8883`
- Username: `AquaSolar`

## Security
Never commit the HiveMQ password. Store it in Render Environment Variables for the web application and in a local, gitignored ESP32 configuration file for the device.

## Topics
- `aquasolar/<deviceId>/telemetry`
- `aquasolar/<deviceId>/status`
- `aquasolar/<deviceId>/command`
