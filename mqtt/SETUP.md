# AquaSolar — installation MQTT

## Architecture

- SIMULATION: dashboard generates values and can publish the same telemetry schema to MQTT.
- REAL: ESP32 reads sensors and publishes `aquasolar/<deviceId>/telemetry`.
- Dashboard subscribes through MQTT over secure WebSockets.
- ESP32 subscribes to `aquasolar/<deviceId>/command` for configuration.

## Demo broker

The project is preconfigured for `test.mosquitto.org`:
- ESP32: MQTT/TLS `8883`
- Browser: MQTT over WebSockets/TLS `wss://test.mosquitto.org:8081/mqtt`

This broker is public and must be considered **demo-only**. Do not use it for sensitive data or production deployment. Mosquitto documents its test server and the WebSocket/TLS listeners. Replace it with a private authenticated broker before production. 

## ESP32

Open `esp32/ranomadio_solar_mqtt.ino` in Arduino IDE and install:
- DHT sensor library by Adafruit
- Adafruit Unified Sensor
- ArduinoJson
- PubSubClient

Set:
```cpp
const char* WIFI_SSID = "YOUR_WIFI_SSID";
const char* WIFI_PASSWORD = "YOUR_WIFI_PASSWORD";
const char* DEVICE_ID = "aquasolar-device01";
```

The ESP32 must connect to a Wi-Fi network that has Internet access because the MQTT broker is remote. If you want a completely offline/local demonstration, run a Mosquitto broker on the same LAN and change the broker host and certificates accordingly.

## Dashboard

The browser uses MQTT over WebSockets. No MQTT username/password is embedded in the frontend for the demo broker. For production, use a broker and authentication model designed for browser clients; never commit privileged broker credentials into frontend JavaScript.

## Important TLS note

The demo firmware uses `secureClient.setInsecure()` to simplify the classroom/demo setup. For production, replace it with CA certificate verification. TLS support and certificate configuration are supported by Mosquitto and the ESP32 MQTT stack.
