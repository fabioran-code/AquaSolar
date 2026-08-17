/*
 * Ranomadio Solar / AquaSolar - ESP32 firmware
 * Phase 1: SEN0169-V2 + DHT11 + HC-SR04 + LEDs + 12V pump/relay
 *
 * API expected by the dashboard:
 *   GET  /api/status
 *   POST /api/config       {"phMin":6.5,"phMax":8.5}
 *   POST /api/pump         {"on":true}
 *   POST /api/auto          {"enabled":true}
 *   GET  /api/health
 *
 * Default Wi-Fi: ESP32 creates AP "Ranomadio-Solar" with password
 * "ranomadio123" and normally uses 192.168.4.1.
 *
 * Libraries (Arduino Library Manager):
 *   DHT sensor library by Adafruit
 *   Adafruit Unified Sensor
 *
 * The SEN0169-V2 analog output is 0-3.0 V and the module accepts 3.3-5.5 V.
 * A two-point pH calibration is implemented below using pH 7 and pH 4.
 */

#include <WiFi.h>
#include <WebServer.h>
#include <Preferences.h>
#include <DHT.h>
#include <ArduinoJson.h>

// ------------------------- Wi-Fi -------------------------
const char* AP_SSID = "Ranomadio-Solar";
const char* AP_PASSWORD = "ranomadio123";

// Optional router credentials. Leave empty to use AP-only mode.
const char* STA_SSID = "";
const char* STA_PASSWORD = "";

WebServer server(80);
Preferences prefs;

// ------------------------- Pins --------------------------
// ESP32 classic / DevKit style pinout.
constexpr uint8_t PH_PIN       = 34;  // ADC1 input only
constexpr uint8_t DHT_PIN      = 4;
constexpr uint8_t TRIG_PIN     = 5;
constexpr uint8_t ECHO_PIN     = 18;
constexpr uint8_t LED_GREEN    = 25;
constexpr uint8_t LED_YELLOW   = 26;
constexpr uint8_t LED_RED      = 27;
constexpr uint8_t PUMP_RELAY   = 23;

// Relay modules vary. Set false if your relay is active LOW.
constexpr bool RELAY_ACTIVE_HIGH = true;

#define DHT_TYPE DHT11
DHT dht(DHT_PIN, DHT_TYPE);

// ------------------------- State -------------------------
float phValue = 7.0f;
float temperature = 25.0f;
float humidity = 0.0f;
float distanceCm = NAN;
float waterLevel = NAN;
float phMin = 6.5f;
float phMax = 8.5f;

// Tank geometry: distance from ultrasonic sensor to bottom in cm.
// Adjust this to your real tank.
float tankHeightCm = 35.0f;

bool pumpOn = false;
bool autoMode = true;
String statusText = "NORMAL";
unsigned long lastSensorRead = 0;
unsigned long pumpStartedAt = 0;
unsigned long lastPumpStopAt = 0;

// Safety limits for the demo/prototype.
constexpr float MIN_WATER_LEVEL = 15.0f; // percent
constexpr unsigned long MAX_PUMP_RUNTIME = 30000UL;
constexpr unsigned long PUMP_COOLDOWN = 10000UL;

// Two-point calibration values for the SEN0169-V2.
// Measure and store the actual voltage at pH 7 and pH 4.
float calV7 = 2.50f;
float calV4 = 3.00f;

// ------------------------- Helpers -----------------------
void setRelay(bool on) {
  digitalWrite(PUMP_RELAY, RELAY_ACTIVE_HIGH ? (on ? HIGH : LOW)
                                              : (on ? LOW : HIGH));
}

void setPump(bool on, bool force = false) {
  if (on) {
    // Never start the pump when the tank is critically low.
    if (!force && !isnan(waterLevel) && waterLevel < MIN_WATER_LEVEL) {
      pumpOn = false;
      setRelay(false);
      return;
    }
    if (!force && millis() - lastPumpStopAt < PUMP_COOLDOWN) return;
    pumpOn = true;
    pumpStartedAt = millis();
  } else {
    pumpOn = false;
    lastPumpStopAt = millis();
  }
  setRelay(pumpOn);
}

void updateLeds() {
  bool critical = statusText == "CRITIQUE" || (!isnan(waterLevel) && waterLevel < MIN_WATER_LEVEL);
  bool abnormal = statusText == "ANORMAL";
  digitalWrite(LED_GREEN, (!critical && !abnormal) ? HIGH : LOW);
  digitalWrite(LED_YELLOW, (abnormal && !critical) ? HIGH : LOW);
  digitalWrite(LED_RED, critical ? HIGH : LOW);
}

float readPHVoltage() {
  // ESP32 ADC is read in calibrated millivolts where supported by the core.
  // GPIO34 is ADC1, so it remains usable while Wi-Fi is active.
  uint32_t mv = analogReadMilliVolts(PH_PIN);
  if (mv == 0) {
    int raw = analogRead(PH_PIN);
    mv = (uint32_t)((raw / 4095.0f) * 3300.0f);
  }
  return mv / 1000.0f;
}

float readPH() {
  const int samples = 15;
  float sum = 0;
  for (int i = 0; i < samples; i++) {
    sum += readPHVoltage();
    delay(4);
  }
  float v = sum / samples;

  // Linear two-point calibration: pH 7 at calV7, pH 4 at calV4.
  float denominator = calV7 - calV4;
  if (fabs(denominator) < 0.05f) return NAN;
  float ph = 7.0f + ((calV7 - v) / denominator) * 3.0f;
  return constrain(ph, 0.0f, 14.0f);
}

float readDistance() {
  digitalWrite(TRIG_PIN, LOW);
  delayMicroseconds(3);
  digitalWrite(TRIG_PIN, HIGH);
  delayMicroseconds(10);
  digitalWrite(TRIG_PIN, LOW);

  unsigned long duration = pulseIn(ECHO_PIN, HIGH, 30000UL);
  if (duration == 0) return NAN;
  float d = duration * 0.0343f / 2.0f;
  if (d < 2.0f || d > 450.0f) return NAN;
  return d;
}

float distanceToLevel(float distance) {
  if (isnan(distance)) return NAN;
  float level = 100.0f - (distance / tankHeightCm) * 100.0f;
  return constrain(level, 0.0f, 100.0f);
}

void readSensors() {
  float newPh = readPH();
  if (!isnan(newPh)) phValue = newPh;

  float t = dht.readTemperature();
  float h = dht.readHumidity();
  if (!isnan(t)) temperature = t;
  if (!isnan(h)) humidity = h;

  distanceCm = readDistance();
  waterLevel = distanceToLevel(distanceCm);

  bool phBad = phValue < phMin || phValue > phMax;
  bool lowLevel = !isnan(waterLevel) && waterLevel < MIN_WATER_LEVEL;

  if (lowLevel) statusText = "CRITIQUE";
  else if (phBad) statusText = "ANORMAL";
  else statusText = "NORMAL";

  if (autoMode) {
    if ((phBad || pumpOn) && !lowLevel) {
      setPump(true);
    } else if (!phBad) {
      setPump(false);
    }
  }

  // Absolute pump runtime protection.
  if (pumpOn && millis() - pumpStartedAt > MAX_PUMP_RUNTIME) {
    setPump(false);
  }

  if (lowLevel) setPump(false);
  updateLeds();
}

// ------------------------- JSON/API ----------------------
void addCors() {
  server.sendHeader("Access-Control-Allow-Origin", "*");
  server.sendHeader("Access-Control-Allow-Methods", "GET,POST,OPTIONS");
  server.sendHeader("Access-Control-Allow-Headers", "Content-Type");
}

void handleOptions() {
  addCors();
  server.send(204);
}

void handleStatus() {
  addCors();
  DynamicJsonDocument doc(1024);
  doc["ph"] = phValue;
  doc["temperature"] = temperature;
  doc["humidity"] = humidity;
  if (isnan(distanceCm)) doc["distance"] = nullptr;
  else doc["distance"] = distanceCm;
  if (isnan(waterLevel)) doc["waterLevel"] = nullptr;
  else doc["waterLevel"] = waterLevel;
  doc["pump"] = pumpOn;
  doc["auto"] = autoMode;
  doc["status"] = statusText;
  doc["timestamp"] = millis();
  // ESP32 DevKit has no reliable battery measurement by default.
  doc["battery"] = nullptr;

  String body;
  serializeJson(doc, body);
  server.send(200, "application/json", body);
}

void handleHealth() {
  addCors();
  DynamicJsonDocument doc(512);
  doc["ok"] = true;
  doc["device"] = "Ranomadio Solar ESP32";
  doc["ip"] = WiFi.softAPIP().toString();
  doc["pump"] = pumpOn;
  doc["uptime"] = millis();
  String body;
  serializeJson(doc, body);
  server.send(200, "application/json", body);
}

void handleConfig() {
  addCors();
  if (server.method() != HTTP_POST) {
    server.send(405, "application/json", "{\"error\":\"POST required\"}");
    return;
  }

  DynamicJsonDocument doc(512);
  DeserializationError err = deserializeJson(doc, server.arg("plain"));
  if (err) {
    server.send(400, "application/json", "{\"error\":\"Invalid JSON\"}");
    return;
  }

  if (doc.containsKey("phMin")) phMin = constrain((float)doc["phMin"], 0.0f, 14.0f);
  if (doc.containsKey("phMax")) phMax = constrain((float)doc["phMax"], 0.0f, 14.0f);
  if (phMin >= phMax) {
    server.send(400, "application/json", "{\"error\":\"phMin must be lower than phMax\"}");
    return;
  }

  prefs.putFloat("phMin", phMin);
  prefs.putFloat("phMax", phMax);
  server.send(200, "application/json", "{\"ok\":true}");
}

void handlePump() {
  addCors();
  DynamicJsonDocument doc(256);
  if (deserializeJson(doc, server.arg("plain"))) {
    server.send(400, "application/json", "{\"error\":\"Invalid JSON\"}");
    return;
  }
  if (!doc.containsKey("on")) {
    server.send(400, "application/json", "{\"error\":\"Missing on\"}");
    return;
  }
  bool requested = doc["on"];
  if (requested && !isnan(waterLevel) && waterLevel < MIN_WATER_LEVEL) {
    setPump(false);
    server.send(409, "application/json", "{\"ok\":false,\"error\":\"Water level too low\"}");
    return;
  }
  setPump(requested);
  server.send(200, "application/json", pumpOn ? "{\"ok\":true,\"pump\":true}" : "{\"ok\":true,\"pump\":false}");
}

void handleAuto() {
  addCors();
  DynamicJsonDocument doc(256);
  if (deserializeJson(doc, server.arg("plain"))) {
    server.send(400, "application/json", "{\"error\":\"Invalid JSON\"}");
    return;
  }
  if (!doc.containsKey("enabled")) {
    server.send(400, "application/json", "{\"error\":\"Missing enabled\"}");
    return;
  }
  autoMode = doc["enabled"];
  prefs.putBool("auto", autoMode);
  if (!autoMode) setPump(false);
  server.send(200, "application/json", autoMode ? "{\"ok\":true,\"auto\":true}" : "{\"ok\":true,\"auto\":false}");
}

void handleNotFound() {
  addCors();
  server.send(404, "application/json", "{\"error\":\"Not found\"}");
}

// ------------------------- Calibration -------------------
void printHelp() {
  Serial.println("\nCommandes de calibration pH:");
  Serial.println("  ph      -> afficher pH et tension");
  Serial.println("  cal7    -> mémoriser la tension actuelle comme pH 7");
  Serial.println("  cal4    -> mémoriser la tension actuelle comme pH 4");
  Serial.println("  save    -> sauvegarder calibration");
  Serial.println("  info    -> afficher configuration");
}

void handleSerial() {
  if (!Serial.available()) return;
  String cmd = Serial.readStringUntil('\n');
  cmd.trim();
  cmd.toLowerCase();

  if (cmd == "ph") {
    float v = readPHVoltage();
    Serial.printf("Tension=%.3f V | pH=%.2f\n", v, phValue);
  } else if (cmd == "cal7") {
    calV7 = readPHVoltage();
    Serial.printf("pH 7 -> %.3f V (non sauvegardé, tapez save)\n", calV7);
  } else if (cmd == "cal4") {
    calV4 = readPHVoltage();
    Serial.printf("pH 4 -> %.3f V (non sauvegardé, tapez save)\n", calV4);
  } else if (cmd == "save") {
    prefs.putFloat("calV7", calV7);
    prefs.putFloat("calV4", calV4);
    Serial.println("Calibration sauvegardée.");
  } else if (cmd == "info") {
    Serial.printf("AP: %s | IP: %s\n", AP_SSID, WiFi.softAPIP().toString().c_str());
    Serial.printf("pH min/max: %.2f / %.2f | V7/V4: %.3f / %.3f\n", phMin, phMax, calV7, calV4);
    Serial.printf("AUTO: %s | Pompe: %s\n", autoMode ? "ON" : "OFF", pumpOn ? "ON" : "OFF");
  } else if (cmd == "help") {
    printHelp();
  }
}

// ------------------------- Setup -------------------------
void setup() {
  Serial.begin(115200);
  delay(300);

  pinMode(PH_PIN, INPUT);
  pinMode(TRIG_PIN, OUTPUT);
  pinMode(ECHO_PIN, INPUT);
  pinMode(LED_GREEN, OUTPUT);
  pinMode(LED_YELLOW, OUTPUT);
  pinMode(LED_RED, OUTPUT);
  pinMode(PUMP_RELAY, OUTPUT);
  setRelay(false);

  analogReadResolution(12);
  analogSetPinAttenuation(PH_PIN, ADC_11db);
  dht.begin();

  prefs.begin("ranomadio", false);
  phMin = prefs.getFloat("phMin", 6.5f);
  phMax = prefs.getFloat("phMax", 8.5f);
  autoMode = prefs.getBool("auto", true);
  calV7 = prefs.getFloat("calV7", 2.50f);
  calV4 = prefs.getFloat("calV4", 3.00f);

  // Always provide the local AP so the dashboard can connect directly.
  WiFi.mode(WIFI_AP_STA);
  WiFi.softAP(AP_SSID, AP_PASSWORD);
  delay(200);

  if (strlen(STA_SSID) > 0) {
    WiFi.begin(STA_SSID, STA_PASSWORD);
    unsigned long start = millis();
    while (WiFi.status() != WL_CONNECTED && millis() - start < 8000) delay(250);
  }

  server.on("/api/status", HTTP_GET, handleStatus);
  server.on("/api/health", HTTP_GET, handleHealth);
  server.on("/api/config", HTTP_POST, handleConfig);
  server.on("/api/pump", HTTP_POST, handlePump);
  server.on("/api/auto", HTTP_POST, handleAuto);
  server.on("/api/status", HTTP_OPTIONS, handleOptions);
  server.on("/api/config", HTTP_OPTIONS, handleOptions);
  server.on("/api/pump", HTTP_OPTIONS, handleOptions);
  server.on("/api/auto", HTTP_OPTIONS, handleOptions);
  server.onNotFound(handleNotFound);
  server.begin();

  Serial.println("\n=== RANOMADIO SOLAR / AQUASOLAR ===");
  Serial.printf("Wi-Fi AP : %s\n", AP_SSID);
  Serial.printf("Mot de passe : %s\n", AP_PASSWORD);
  Serial.printf("IP ESP32 : %s\n", WiFi.softAPIP().toString().c_str());
  if (WiFi.status() == WL_CONNECTED) Serial.printf("IP routeur : %s\n", WiFi.localIP().toString().c_str());
  Serial.println("API : http://192.168.4.1/api/status");
  printHelp();

  readSensors();
}

void loop() {
  server.handleClient();
  handleSerial();

  if (millis() - lastSensorRead >= 2000UL) {
    lastSensorRead = millis();
    readSensors();
  }
}
