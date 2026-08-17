/*
 * RANOMADIO SOLAR - ESP32 firmware
 * Phase 1: SEN0169-V2 + DHT11 + HC-SR04 + LEDs + 12V pump
 *
 * API:
 *   GET  /api/status
 *   POST /api/config   {"phMin":6.5,"phMax":8.5}
 *
 * The pump decision is LOCAL on the ESP32. The web application
 * only supervises the measurements and configuration.
 */
#include <Arduino.h>
#include <WiFi.h>
#include <WebServer.h>
#include <DHT.h>
#include <ArduinoJson.h>

#define PH_PIN 34
#define DHT_PIN 4
#define DHT_TYPE DHT11
#define TRIG_PIN 5
#define ECHO_PIN 18
#define LED_RED 25
#define LED_YELLOW 26
#define LED_GREEN 27
#define PUMP_PIN 23

const char* AP_SSID = "Ranomadio-ESP32";
const char* AP_PASSWORD = "Ranomadio2026";

const unsigned long SENSOR_INTERVAL = 3000;
const unsigned long MAX_PUMP_TIME = 30000;
const float PH_CRITICAL_LOW = 5.5;
const float PH_CRITICAL_HIGH = 9.5;
const float CRITICAL_WATER_DISTANCE = 35.0;
const float TANK_HEIGHT_CM = 35.0;

float phMin = 6.5;
float phMax = 8.5;
float phSlope = -5.70;
float phOffset = 21.34;

WebServer server(80);
DHT dht(DHT_PIN, DHT_TYPE);

float ph = NAN;
float temperature = NAN;
float humidity = NAN;
float distanceCm = NAN;
float waterLevel = NAN;
bool pumpState = false;
unsigned long pumpStart = 0;
unsigned long lastRead = 0;
String waterStatus = "INIT";

void setLeds(bool red, bool yellow, bool green) {
  digitalWrite(LED_RED, red);
  digitalWrite(LED_YELLOW, yellow);
  digitalWrite(LED_GREEN, green);
}

float readPHVoltage() {
  const int samples = 20;
  uint32_t total = 0;
  for (int i = 0; i < samples; i++) {
    total += analogRead(PH_PIN);
    delay(3);
  }
  return (total / (float)samples) * 3.3f / 4095.0f;
}

float readPH() {
  return phSlope * readPHVoltage() + phOffset;
}

float readDistance() {
  digitalWrite(TRIG_PIN, LOW);
  delayMicroseconds(2);
  digitalWrite(TRIG_PIN, HIGH);
  delayMicroseconds(10);
  digitalWrite(TRIG_PIN, LOW);
  unsigned long duration = pulseIn(ECHO_PIN, HIGH, 30000);
  if (!duration) return NAN;
  return duration / 58.0f;
}

void stopPump(const char* reason) {
  if (pumpState) Serial.printf("POMPE OFF: %s\n", reason);
  digitalWrite(PUMP_PIN, LOW);
  pumpState = false;
}

void startPump() {
  if (!pumpState) {
    digitalWrite(PUMP_PIN, HIGH);
    pumpState = true;
    pumpStart = millis();
    Serial.println("POMPE ON: traitement automatique");
  }
}

void updateStatusAndPump() {
  if (isnan(ph) || isnan(distanceCm)) {
    waterStatus = "ERREUR_CAPTEUR";
    stopPump("capteur invalide");
    setLeds(true, false, false);
    return;
  }

  if (distanceCm >= CRITICAL_WATER_DISTANCE) {
    waterStatus = "CRITIQUE";
    stopPump("niveau d'eau critique");
    setLeds(true, false, false);
    return;
  }

  bool badPh = ph < phMin || ph > phMax;
  bool criticalPh = ph < PH_CRITICAL_LOW || ph > PH_CRITICAL_HIGH;

  if (criticalPh) waterStatus = "CRITIQUE";
  else if (badPh) waterStatus = "ANORMAL";
  else waterStatus = "NORMAL";

  if (badPh) startPump();
  else stopPump("pH revenu dans la plage");

  if (pumpState) setLeds(false, true, false);
  else if (badPh) setLeds(true, false, false);
  else setLeds(false, false, true);
}

void readSensors() {
  ph = readPH();
  temperature = dht.readTemperature();
  humidity = dht.readHumidity();
  distanceCm = readDistance();
  waterLevel = isnan(distanceCm) ? NAN : max(0.0f, min(100.0f, 100.0f - (distanceCm / TANK_HEIGHT_CM) * 100.0f));
  updateStatusAndPump();

  Serial.printf("pH=%.2f | Temp=%.1f C | Hum=%.1f %% | Distance=%.1f cm | Niveau=%.0f %% | Pompe=%s | Etat=%s\n",
    ph, temperature, humidity, distanceCm, waterLevel, pumpState ? "ON" : "OFF", waterStatus.c_str());
}

void sendStatus() {
  StaticJsonDocument<512> doc;
  doc["device"] = "RANOMADIO-ESP32-01";
  doc["ph"] = isnan(ph) ? -1 : ph;
  doc["temperature"] = isnan(temperature) ? -1 : temperature;
  doc["humidity"] = isnan(humidity) ? -1 : humidity;
  doc["distance"] = isnan(distanceCm) ? -1 : distanceCm;
  doc["waterLevel"] = isnan(waterLevel) ? -1 : waterLevel;
  doc["pump"] = pumpState;
  doc["status"] = waterStatus;
  doc["phMin"] = phMin;
  doc["phMax"] = phMax;
  doc["battery"] = nullptr; // Phase 1: no battery voltage sensor
  doc["timestamp"] = millis();
  String json;
  serializeJson(doc, json);
  server.sendHeader("Access-Control-Allow-Origin", "*");
  server.sendHeader("Cache-Control", "no-store");
  server.send(200, "application/json", json);
}

void handleConfig() {
  if (!server.hasArg("plain")) {
    server.send(400, "application/json", "{\"error\":\"body required\"}");
    return;
  }
  StaticJsonDocument<256> doc;
  if (deserializeJson(doc, server.arg("plain"))) {
    server.send(400, "application/json", "{\"error\":\"invalid json\"}");
    return;
  }
  if (doc["phMin"].is<float>()) phMin = doc["phMin"].as<float>();
  if (doc["phMax"].is<float>()) phMax = doc["phMax"].as<float>();
  if (phMin >= phMax) {
    server.send(400, "application/json", "{\"error\":\"invalid thresholds\"}");
    return;
  }
  server.sendHeader("Access-Control-Allow-Origin", "*");
  server.send(200, "application/json", "{\"ok\":true}");
}

void handleOptions() {
  server.sendHeader("Access-Control-Allow-Origin", "*");
  server.sendHeader("Access-Control-Allow-Methods", "GET,POST,OPTIONS");
  server.sendHeader("Access-Control-Allow-Headers", "Content-Type");
  server.send(204);
}

void setup() {
  Serial.begin(115200);
  pinMode(PH_PIN, INPUT);
  pinMode(TRIG_PIN, OUTPUT);
  pinMode(ECHO_PIN, INPUT);
  pinMode(LED_RED, OUTPUT);
  pinMode(LED_YELLOW, OUTPUT);
  pinMode(LED_GREEN, OUTPUT);
  pinMode(PUMP_PIN, OUTPUT);
  digitalWrite(PUMP_PIN, LOW);
  setLeds(false, false, false);
  dht.begin();

  WiFi.mode(WIFI_AP);
  WiFi.softAP(AP_SSID, AP_PASSWORD);
  Serial.println("\n=== RANOMADIO SOLAR ===");
  Serial.print("Wi-Fi SSID: "); Serial.println(AP_SSID);
  Serial.print("Mot de passe: "); Serial.println(AP_PASSWORD);
  Serial.print("API: http://"); Serial.println(WiFi.softAPIP());

  server.on("/api/status", HTTP_GET, sendStatus);
  server.on("/api/config", HTTP_POST, handleConfig);
  server.on("/api/config", HTTP_OPTIONS, handleOptions);
  server.onNotFound([](){
    server.sendHeader("Access-Control-Allow-Origin", "*");
    server.send(404, "application/json", "{\"error\":\"not found\"}");
  });
  server.begin();
  readSensors();
}

void loop() {
  server.handleClient();
  if (millis() - lastRead >= SENSOR_INTERVAL) {
    lastRead = millis();
    readSensors();
  }
  if (pumpState && millis() - pumpStart >= MAX_PUMP_TIME) {
    stopPump("temps maximal de sécurité atteint");
  }
  delay(2);
}
