/* AquaSolar - ESP32 MQTT firmware
 * Real mode: ESP32 -> EMQX Cloud -> AquaSolar dashboard
 * Sensors: SEN0169-V2, DHT11, HC-SR04, LEDs, 3-6V 120L/h micro submersible pump.
 *
 * Pump hardware:
 * - Do NOT power the pump directly from an ESP32 GPIO.
 * - Use a separate regulated 5V supply (within the pump's 3-6V range).
 * - Control the pump with a logic-level N-MOSFET or a suitable transistor/relay.
 * - Connect ESP32 GND and the pump supply GND together when using a MOSFET/transistor.
 * - Add a flyback diode across the pump motor (cathode to +5V, anode to the MOSFET/drain side).
 * - GPIO 23 remains the pump control signal.
 *
 * Libraries: DHT sensor library, Adafruit Unified Sensor, ArduinoJson, PubSubClient.
 * IMPORTANT: replace WIFI and EMQX credentials before flashing.
 * For this demo the TLS client uses setInsecure(). For production, install
 * and validate the EMQX CA certificate instead.
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

#define PUMP_SUPPLY_VOLTAGE 5.0f

const char* WIFI_SSID = "YOUR_WIFI_SSID";
const char* WIFI_PASSWORD = "YOUR_WIFI_PASSWORD";
// EMQX Cloud endpoint shown in Deployment Overview -> MQTT Connection Information
const char* MQTT_HOST = "k02a8067.ala.eu-central-1.emqxsl.com";
const uint16_t MQTT_PORT = 8883;
const char* MQTT_USER = "AquaSolar";
const char* MQTT_PASSWORD = "YOUR_EMQX_PASSWORD";
const char* DEVICE_ID = "aquasolar-device01";

const unsigned long SENSOR_INTERVAL = 3000;
const unsigned long MAX_PUMP_TIME = 30000;
const float PH_CRITICAL_LOW = 5.5;
const float PH_CRITICAL_HIGH = 9.5;
const float CRITICAL_WATER_DISTANCE = 35.0;
const float TANK_HEIGHT_CM = 35.0;

float phMin = 6.5, phMax = 8.5;
float phSlope = -5.70, phOffset = 21.34;
float ph=NAN, temperature=NAN, humidity=NAN, distanceCm=NAN, waterLevel=NAN;
bool pumpState=false;
unsigned long pumpStart=0, lastRead=0, lastPublish=0;
String waterStatus="INIT";

WiFiClientSecure secureClient;
PubSubClient mqtt(secureClient);
DHT dht(DHT_PIN,DHT_TYPE);

String topic(const char* suffix){return String("aquasolar/")+DEVICE_ID+"/"+suffix;}
void setLeds(bool red,bool yellow,bool green){digitalWrite(LED_RED,red);digitalWrite(LED_YELLOW,yellow);digitalWrite(LED_GREEN,green);}
float readPHVoltage(){uint32_t total=0;for(int i=0;i<20;i++){total+=analogRead(PH_PIN);delay(3);}return(total/20.0f)*3.3f/4095.0f;}
float readPH(){return phSlope*readPHVoltage()+phOffset;}
float readDistance(){digitalWrite(TRIG_PIN,LOW);delayMicroseconds(2);digitalWrite(TRIG_PIN,HIGH);delayMicroseconds(10);digitalWrite(TRIG_PIN,LOW);unsigned long duration=pulseIn(ECHO_PIN,HIGH,30000);return duration?duration/58.0f:NAN;}
void stopPump(const char* reason){if(pumpState)Serial.printf("POMPE OFF: %s\n",reason);digitalWrite(PUMP_PIN,LOW);pumpState=false;}
void startPump(){if(!pumpState){digitalWrite(PUMP_PIN,HIGH);pumpState=true;pumpStart=millis();Serial.println("POMPE ON: traitement automatique");}}
void updateStatusAndPump(){if(isnan(ph)||isnan(distanceCm)){waterStatus="ERREUR_CAPTEUR";stopPump("capteur invalide");setLeds(true,false,false);return;}if(distanceCm>=CRITICAL_WATER_DISTANCE){waterStatus="CRITIQUE";stopPump("niveau critique");setLeds(true,false,false);return;}bool badPh=ph<phMin||ph>phMax;bool criticalPh=ph<PH_CRITICAL_LOW||ph>PH_CRITICAL_HIGH;waterStatus=criticalPh?"CRITIQUE":(badPh?"ANORMAL":"NORMAL");if(badPh)startPump();else stopPump("pH normal");if(pumpState)setLeds(false,true,false);else if(badPh)setLeds(true,false,false);else setLeds(false,false,true);}
void readSensors(){ph=readPH();temperature=dht.readTemperature();humidity=dht.readHumidity();distanceCm=readDistance();waterLevel=isnan(distanceCm)?NAN:max(0.0f,min(100.0f,100.0f-(distanceCm/TANK_HEIGHT_CM)*100.0f));updateStatusAndPump();Serial.printf("pH=%.2f Temp=%.1f Hum=%.1f Distance=%.1f Level=%.0f Pump=%s Status=%s\n",ph,temperature,humidity,distanceCm,waterLevel,pumpState?"ON":"OFF",waterStatus.c_str());}
void publishTelemetry(){StaticJsonDocument<512> doc;doc["device"]=DEVICE_ID;doc["ph"]=isnan(ph)?-1:ph;doc["temperature"]=isnan(temperature)?-1:temperature;doc["humidity"]=isnan(humidity)?-1:humidity;doc["distance"]=isnan(distanceCm)?-1:distanceCm;doc["waterLevel"]=isnan(waterLevel)?-1:waterLevel;doc["pump"]=pumpState;doc["status"]=waterStatus;doc["phMin"]=phMin;doc["phMax"]=phMax;doc["pumpVoltage"]=PUMP_SUPPLY_VOLTAGE;doc["battery"]=nullptr;doc["timestamp"]=millis();String payload;serializeJson(doc,payload);mqtt.publish(topic("telemetry").c_str(),payload.c_str(),false);mqtt.publish(topic("status").c_str(),waterStatus.c_str(),false);}
void handleCommand(char* payload){StaticJsonDocument<256> doc;if(deserializeJson(doc,payload))return;const char* type=doc["type"]|"";if(strcmp(type,"config")==0){float a=doc["phMin"]|phMin;float b=doc["phMax"]|phMax;if(a<b){phMin=a;phMax=b;Serial.printf("Seuils MQTT: %.2f - %.2f\n",phMin,phMax);}}else if(strcmp(type,"pump")==0){bool requested=doc["on"]|false;if(requested&&waterStatus!="CRITIQUE")startPump();else stopPump("commande MQTT ou sécurité");}}
void mqttCallback(char* t,byte* payload,unsigned int length){if(String(t)==topic("command")){String s;for(unsigned int i=0;i<length;i++)s+=(char)payload[i];char buf[512];s.toCharArray(buf,sizeof(buf));handleCommand(buf);}}
void connectWiFi(){WiFi.mode(WIFI_STA);WiFi.begin(WIFI_SSID,WIFI_PASSWORD);Serial.print("Wi-Fi");unsigned long start=millis();while(WiFi.status()!=WL_CONNECTED&&millis()-start<20000){delay(500);Serial.print('.');}Serial.println();if(WiFi.status()==WL_CONNECTED){Serial.print("IP: ");Serial.println(WiFi.localIP());}else Serial.println("Wi-Fi non connecté");}
void connectMQTT(){if(WiFi.status()!=WL_CONNECTED)return;if(mqtt.connected())return;String cid=String(DEVICE_ID)+"-"+String((uint32_t)ESP.getEfuseMac(),HEX);Serial.print("MQTT/EMQX...");if(mqtt.connect(cid.c_str(),MQTT_USER,MQTT_PASSWORD)){Serial.println("OK");mqtt.subscribe(topic("command").c_str(),0);mqtt.publish(topic("status").c_str(),"ONLINE",true);publishTelemetry();}else Serial.printf("échec rc=%d\n",mqtt.state());}
void setup(){Serial.begin(115200);pinMode(PH_PIN,INPUT);pinMode(TRIG_PIN,OUTPUT);pinMode(ECHO_PIN,INPUT);pinMode(LED_RED,OUTPUT);pinMode(LED_YELLOW,OUTPUT);pinMode(LED_GREEN,OUTPUT);pinMode(PUMP_PIN,OUTPUT);digitalWrite(PUMP_PIN,LOW);setLeds(false,false,false);dht.begin();connectWiFi();secureClient.setInsecure();mqtt.setServer(MQTT_HOST,MQTT_PORT);mqtt.setCallback(mqttCallback);mqtt.setBufferSize(1024);readSensors();}
void loop(){if(WiFi.status()!=WL_CONNECTED)connectWiFi();if(!mqtt.connected())connectMQTT();mqtt.loop();if(millis()-lastRead>=SENSOR_INTERVAL){lastRead=millis();readSensors();}if(millis()-lastPublish>=SENSOR_INTERVAL&&mqtt.connected()){lastPublish=millis();publishTelemetry();}if(pumpState&&millis()-pumpStart>=MAX_PUMP_TIME)stopPump("temps maximal de sécurité");delay(10);}