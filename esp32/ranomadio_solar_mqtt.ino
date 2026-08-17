/*
 * AquaSolar - ESP32 MQTT firmware
 * Sensors: SEN0169-V2 + DHT11 + HC-SR04 + LEDs + 12V pump
 *
 * Real architecture:
 * ESP32 -> MQTT/TLS -> broker -> AquaSolar dashboard (MQTT/WSS)
 *
 * DEMO BROKER: test.mosquitto.org
 * Replace WIFI_* and MQTT_* values before deployment.
 * The public test broker is for demonstrations only.
 */
#include <Arduino.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <PubSubClient.h>
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

// ===== Wi-Fi: use a router/hotspot with Internet access =====
const char* WIFI_SSID = "YOUR_WIFI_SSID";
const char* WIFI_PASSWORD = "YOUR_WIFI_PASSWORD";

// ===== MQTT demo broker =====
const char* MQTT_HOST = "test.mosquitto.org";
const uint16_t MQTT_PORT = 8883;
const char* MQTT_USER = "";
const char* MQTT_PASSWORD = "";
const char* DEVICE_ID = "aquasolar-device01";

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

WiFiClientSecure secureClient;
PubSubClient mqtt(secureClient);
DHT dht(DHT_PIN, DHT_TYPE);

float ph=NAN, temperature=NAN, humidity=NAN, distanceCm=NAN, waterLevel=NAN;
bool pumpState=false, autoMode=true;
unsigned long pumpStart=0, lastRead=0, lastPublish=0;
String waterStatus="INIT";

String topicTelemetry(){return String("aquasolar/")+DEVICE_ID+"/telemetry";}
String topicStatus(){return String("aquasolar/")+DEVICE_ID+"/status";}
String topicCommand(){return String("aquasolar/")+DEVICE_ID+"/command";}

void setLeds(bool red,bool yellow,bool green){digitalWrite(LED_RED,red);digitalWrite(LED_YELLOW,yellow);digitalWrite(LED_GREEN,green);}

float readPHVoltage(){uint32_t total=0;for(int i=0;i<20;i++){total+=analogRead(PH_PIN);delay(3);}return(total/20.0f)*3.3f/4095.0f;}
float readPH(){return phSlope*readPHVoltage()+phOffset;}

float readDistance(){
  digitalWrite(TRIG_PIN,LOW);delayMicroseconds(2);digitalWrite(TRIG_PIN,HIGH);delayMicroseconds(10);digitalWrite(TRIG_PIN,LOW);
  unsigned long duration=pulseIn(ECHO_PIN,HIGH,30000);if(!duration)return NAN;return duration/58.0f;
}

void stopPump(const char* reason){if(pumpState)Serial.printf("POMPE OFF: %s\n",reason);digitalWrite(PUMP_PIN,LOW);pumpState=false;}
void startPump(){if(!pumpState){digitalWrite(PUMP_PIN,HIGH);pumpState=true;pumpStart=millis();Serial.println("POMPE ON");}}

void updateStatusAndPump(){
  if(isnan(ph)||isnan(distanceCm)){waterStatus="ERREUR_CAPTEUR";stopPump("capteur invalide");setLeds(true,false,false);return;}
  if(distanceCm>=CRITICAL_WATER_DISTANCE){waterStatus="CRITIQUE";stopPump("niveau critique");setLeds(true,false,false);return;}
  bool badPh=ph<phMin||ph>phMax;bool criticalPh=ph<PH_CRITICAL_LOW||ph>PH_CRITICAL_HIGH;
  waterStatus=criticalPh?"CRITIQUE":badPh?"ANORMAL":"NORMAL";
  if(autoMode){if(badPh)startPump();else stopPump("pH normal");}
  if(pumpState)setLeds(false,true,false);else if(badPh)setLeds(true,false,false);else setLeds(false,false,true);
}

void readSensors(){
  ph=readPH();temperature=dht.readTemperature();humidity=dht.readHumidity();distanceCm=readDistance();
  waterLevel=isnan(distanceCm)?NAN:max(0.0f,min(100.0f,100.0f-(distanceCm/TANK_HEIGHT_CM)*100.0f));
  updateStatusAndPump();
  Serial.printf("pH %.2f | T %.1f | H %.1f | Level %.0f | Pump %s | %s\n",ph,temperature,humidity,waterLevel,pumpState?"ON":"OFF",waterStatus.c_str());
}

void publishTelemetry(){
  if(!mqtt.connected())return;
  StaticJsonDocument<512> doc;
  doc["device"]=DEVICE_ID;doc["ph"]=isnan(ph)?-1:ph;doc["temperature"]=isnan(temperature)?-1:temperature;doc["humidity"]=isnan(humidity)?-1:humidity;
  doc["distance"]=isnan(distanceCm)?-1:distanceCm;doc["waterLevel"]=isnan(waterLevel)?-1:waterLevel;doc["pump"]=pumpState;doc["auto"]=autoMode;doc["status"]=waterStatus;
  doc["phMin"]=phMin;doc["phMax"]=phMax;doc["battery"]=nullptr;doc["timestamp"]=millis();
  char out[512];size_t n=serializeJson(doc,out,sizeof(out));mqtt.publish(topicTelemetry().c_str(),out,n,false);mqtt.publish(topicStatus().c_str(),out,n,true);
}

void handleCommand(char* topic,byte* payload,unsigned int length){
  StaticJsonDocument<256> doc;if(deserializeJson(doc,payload,length))return;
  if(doc["auto"].is<bool>()){autoMode=doc["auto"].as<bool>();if(!autoMode)stopPump("mode manuel");}
  if(doc["config"].is<JsonObject>()){JsonObject c=doc["config"];if(c["phMin"].is<float>())phMin=c["phMin"].as<float>();if(c["phMax"].is<float>())phMax=c["phMax"].as<float>();if(phMin>=phMax){phMin=6.5;phMax=8.5;}}
  readSensors();publishTelemetry();
}

void connectWiFi(){
  WiFi.mode(WIFI_STA);WiFi.begin(WIFI_SSID,WIFI_PASSWORD);Serial.print("Wi-Fi");
  unsigned long start=millis();while(WiFi.status()!=WL_CONNECTED&&millis()-start<20000){delay(500);Serial.print(".");}
  Serial.println();if(WiFi.status()==WL_CONNECTED){Serial.print("IP: ");Serial.println(WiFi.localIP());}else Serial.println("Wi-Fi non connecté");
}

void connectMQTT(){
  if(WiFi.status()!=WL_CONNECTED)return;
  while(!mqtt.connected()){
    Serial.print("MQTT...");
    bool ok=MQTT_USER&&strlen(MQTT_USER)>0?mqtt.connect(DEVICE_ID,MQTT_USER,MQTT_PASSWORD,topicStatus().c_str(),0,true,"{\"status\":\"OFFLINE\"}"):mqtt.connect(DEVICE_ID,topicStatus().c_str(),0,true,"{\"status\":\"OFFLINE\"}");
    if(ok){Serial.println("OK");mqtt.subscribe(topicCommand().c_str());publishTelemetry();}
    else{Serial.printf("ECHEC rc=%d\n",mqtt.state());delay(3000);}
  }
}

void setup(){
  Serial.begin(115200);pinMode(PH_PIN,INPUT);pinMode(TRIG_PIN,OUTPUT);pinMode(ECHO_PIN,INPUT);pinMode(LED_RED,OUTPUT);pinMode(LED_YELLOW,OUTPUT);pinMode(LED_GREEN,OUTPUT);pinMode(PUMP_PIN,OUTPUT);digitalWrite(PUMP_PIN,LOW);setLeds(false,false,false);dht.begin();
  secureClient.setInsecure(); // DEMO ONLY. Production: load and verify the broker CA certificate.
  mqtt.setServer(MQTT_HOST,MQTT_PORT);mqtt.setCallback(handleCommand);mqtt.setBufferSize(1024);
  Serial.println("=== AQUASOLAR MQTT ESP32 ===");Serial.print("Device: ");Serial.println(DEVICE_ID);Serial.print("Telemetry: ");Serial.println(topicTelemetry());
  connectWiFi();readSensors();
}

void loop(){
  if(WiFi.status()!=WL_CONNECTED)connectWiFi();
  if(!mqtt.connected())connectMQTT();
  mqtt.loop();
  if(millis()-lastRead>=SENSOR_INTERVAL){lastRead=millis();readSensors();}
  if(millis()-lastPublish>=SENSOR_INTERVAL){lastPublish=millis();publishTelemetry();}
  if(pumpState&&millis()-pumpStart>=MAX_PUMP_TIME)stopPump("timeout securite");
  delay(2);
}
