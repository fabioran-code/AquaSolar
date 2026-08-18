# AquaSolar + EMQX Cloud

## Browser / Render
- Broker: EMQX Cloud
- Secure WebSocket port: `8084`
- WebSocket path: `/mqtt`
- URL: `wss://YOUR_EMQX_ENDPOINT:8084/mqtt`
- Username: `AquaSolar`

The endpoint is configured in `mqtt/emqx-config.example.js`. Do not commit the real MQTT password.

## ESP32
- Broker: EMQX Cloud
- MQTT over TLS port: `8883`
- Username: `AquaSolar`
- Password: EMQX Cloud authentication credential

Before flashing, replace `YOUR_EMQX_ENDPOINT`, Wi-Fi credentials and the MQTT password in the ESP32 firmware.

## Security
Never commit the real EMQX password. For the web application, the password is requested at runtime. For the ESP32, keep production credentials in a local configuration file that is excluded from Git.

For the current demonstration firmware, TLS uses `setInsecure()`. For a production deployment, install and validate the EMQX CA certificate instead.

## Topics
- `aquasolar/<deviceId>/telemetry`
- `aquasolar/<deviceId>/status`
- `aquasolar/<deviceId>/command`

## EMQX Cloud setup
Create an MQTT authentication credential for AquaSolar and make sure the ACL/rules allow:
- Publish: `aquasolar/+/telemetry`
- Publish: `aquasolar/+/status`
- Subscribe: `aquasolar/+/command`
- WebSocket access on port `8084`
- MQTT/TLS access on port `8883`

Use the exact endpoint, username and password displayed by your EMQX Cloud deployment.