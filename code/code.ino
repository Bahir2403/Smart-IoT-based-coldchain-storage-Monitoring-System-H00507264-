#include <WiFi.h>
#include <PubSubClient.h>
#include <DHT.h>
#include <Adafruit_NeoPixel.h>
#include <ArduinoJson.h>

#define WIFI_SSID      "Amitabh Mama"
#define WIFI_PASSWORD  "gearboys@123"

#define MQTT_BROKER  "test.mosquitto.org"
#define   MQTT_PORT    1883

#define TOPIC_DATA    "freezer/temp"
#define TOPIC_CMD     "freezer/cmd"
#define TOPIC_SYSTEM  "freezer/system"
#define TOPIC_STATUS  "freezer/status"

#define DHT_PIN    4        
#define DHT_TYPE   DHT11
#define NEOPIXEL_PIN 5       
#define STATUS_LED 2          

DHT dht(DHT_PIN, DHT_TYPE);
Adafruit_NeoPixel pixel(1, NEOPIXEL_PIN, NEO_GRB + NEO_KHZ800);
WiFiClient wifiClient;
PubSubClient mqtt(wifiClient);

unsigned long lastReading = 0;
const unsigned long READ_INTERVAL = 10000;
bool ledState = false;
bool systemEnabled = true;

void connectWiFi();
void reconnectMQTT();
void publishData();
void handleMQTT(char* topic, byte* payload, unsigned int length);
void setPixelColor(uint8_t r, uint8_t g, uint8_t b);
void updatePixelForTemp(float temp);

void setup() {
  Serial.begin(115200);
  pinMode(STATUS_LED, OUTPUT);
  digitalWrite(STATUS_LED, LOW);

  dht.begin();
  pixel.begin();
  setPixelColor(255, 255, 0); 

  connectWiFi();

  mqtt.setServer(MQTT_BROKER, MQTT_PORT);
  mqtt.setCallback(handleMQTT);
}

void loop() {
  if (!mqtt.connected()) reconnectMQTT();
  mqtt.loop();

  if (millis() - lastReading > READ_INTERVAL && systemEnabled) {
    lastReading = millis();
    publishData();
  }
}

void connectWiFi() {
  Serial.printf("Connecting to Wi-Fi: %s\n", WIFI_SSID);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nWi-Fi connected!");
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());
  setPixelColor(0, 0, 255); 
}

void reconnectMQTT() {
  while (!mqtt.connected()) {
    Serial.print("Connecting to MQTT...");
    String clientId = "ESP32C3_Freezer_" + String((uint32_t)ESP.getEfuseMac(), HEX);

    if (mqtt.connect(clientId.c_str(), NULL, NULL, TOPIC_STATUS, 0, true, "offline")) {
      Serial.println("connected");
      mqtt.subscribe(TOPIC_CMD);
      mqtt.subscribe(TOPIC_SYSTEM);
      mqtt.publish(TOPIC_STATUS, "online", true);
      setPixelColor(0, 255, 0); 
    } else {
      Serial.print("failed, rc=");
      Serial.print(mqtt.state());
      Serial.println(" retrying in 5s");
      setPixelColor(255, 0, 0); 
      delay(5000);
    }
  }
}

void handleMQTT(char* topic, byte* payload, unsigned int length) {
  String message;
  for (unsigned int i = 0; i < length; i++) message += (char)payload[i];
  Serial.printf("MQTT [%s] %s\n", topic, message.c_str());

  if (String(topic) == TOPIC_CMD) {
    if (message == "led_on") {
      digitalWrite(STATUS_LED, HIGH);
      ledState = true;
    } else if (message == "led_off") {
      digitalWrite(STATUS_LED, LOW);
      ledState = false;
    }
  } 
  else if (String(topic) == TOPIC_SYSTEM) {
    if (message == "system_on") {
      systemEnabled = true;
      setPixelColor(0, 255, 0); 
    } else if (message == "system_off") {
      systemEnabled = false;
      setPixelColor(100, 100, 100); 
    }
  }
}

void publishData() {
  float temp = dht.readTemperature();
  float humidity = dht.readHumidity();

  if (isnan(temp) || isnan(humidity)) {
    Serial.println("DHT11 read failed!");
    return;
  }

  updatePixelForTemp(temp);

  DynamicJsonDocument doc(256);
  doc["device"] = "Freezer_Unit1";
  doc["temp"] = temp;
  doc["humidity"] = humidity;
  doc["led_status"] = ledState ? "on" : "off";
  doc["uptime"] = millis() / 1000;
  doc["ip"] = WiFi.localIP().toString();

  String color;
  if (temp >= 0 && temp <= 5) color = "green";
  else if (temp > 5 && temp < 8) color = "orange";
  else color = "red";
  doc["led_color"] = color;

  char payload[256];
  serializeJson(doc, payload);
  
  if (mqtt.publish(TOPIC_DATA, payload)) {
    Serial.println("Published: " + String(payload));
  } else {
    Serial.println("Publish failed!");
  }
}

void setPixelColor(uint8_t r, uint8_t g, uint8_t b) {
  pixel.setPixelColor(0, pixel.Color(r, g, b));
  pixel.show();
}

void updatePixelForTemp(float temp) {
  if (temp >= 0 && temp <= 5)      setPixelColor(0, 255, 0);     
  else if (temp > 5 && temp < 8)   setPixelColor(255, 100, 0);   
  else                             setPixelColor(255, 0, 0);     
}
