#include <Arduino.h>
#include <WiFi.h> 
#include <PubSubClient.h>
#include <ArduinoJson.h> 
#include "config.h"
#include "DHT.h"           
#include <Adafruit_Sensor.h> 
#include <LiquidCrystal_I2C.h>

// --- 1. Khởi tạo thiết bị ---
// Sử dụng các định nghĩa chân từ file config.h
DHT dht(DHT_PIN, DHT22); 
LiquidCrystal_I2C lcd(LCD_ADDR, LCD_COLS, LCD_ROWS);

bool isLcdBacklightOn = false; 

// --- 2. Ngưỡng tiêu chuẩn ---
int auto_soil_min = 30; 
int auto_temp_max = 80;     
int auto_hum_min = 0;       

WiFiClient wifiClient;
PubSubClient mqttClient(wifiClient);

unsigned long lastMsgTime = 0;
const long interval = 5000; 

int soilMoisture = 0;
int waterLevel = 0;
float temp = 0;
float hum = 0;

DynamicJsonDocument doc(512);

// --- 3. Các hàm chức năng ---

void setup_wifi() {
  delay(10);
  Serial.println("\nConnecting to WiFi...");
  WiFi.begin(WIFI_SSID, WIFI_PASS);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nWiFi connected");
}

void callback(char* topic, byte* payload, unsigned int length) {
  Serial.print("Message arrived [");
  Serial.print(topic);
  Serial.println("]");

  DeserializationError error = deserializeJson(doc, payload, length);
  if (error) return;

  const char* command_type = doc["type"] | "UNKNOWN";

  if (String(topic) == TOPIC_CONTROL_PUMP) {
    if (strcmp(command_type, "SETTING_CHANGE") == 0) {
      auto_soil_min = doc["data"]["soil_min"] | 30;
      auto_temp_max = doc["data"]["temp_max"] | 35;
      auto_hum_min = doc["data"]["hum_min"] | 50;
      Serial.println("Settings Updated");
    } 
    else if (strcmp(command_type, "WATERING") == 0) {
      int duration = doc["duration_s"] | 0;
      if (duration > 0) {
        digitalWrite(BUZZER_PIN, HIGH);
        delay(500);
        digitalWrite(BUZZER_PIN, LOW);
        Serial.printf("Watering for %d s\n", duration);
        delay(duration * 1000); 
      }
    }
  }
}

void reconnect() {
  while (!mqttClient.connected()) {
    Serial.print("Attempting MQTT connection...");
    String clientId = "ESP32Client-" + String(random(0xffff), HEX);
    if (mqttClient.connect(clientId.c_str())) {
      Serial.println("connected");
      mqttClient.subscribe(TOPIC_CONTROL_PUMP);
    } else {
      delay(5000);
    }
  }
}

long readDistanceCm() {
  digitalWrite(TRIG_PIN, LOW);
  delayMicroseconds(2);
  digitalWrite(TRIG_PIN, HIGH);
  delayMicroseconds(10);
  digitalWrite(TRIG_PIN, LOW);
  long duration = pulseIn(ECHO_PIN, HIGH);
  return duration * 0.034 / 2;
}

void controlAndDisplayLCD() {
  long distance = readDistanceCm();
  if (distance <= MAX_DISTANCE_CM) {
    if (!isLcdBacklightOn) {
      lcd.backlight();
      isLcdBacklightOn = true;
      lcd.clear();
    }
  } else {
    if (isLcdBacklightOn) {
      lcd.noBacklight();
      isLcdBacklightOn = false;
    }
    return;
  }

  if (isLcdBacklightOn) {
    lcd.setCursor(0, 0);
    lcd.print("T:"); lcd.print(temp, 1); lcd.print("C ");
    lcd.setCursor(10, 0);
    lcd.print("H:"); lcd.print(hum, 1); lcd.print("%");
    lcd.setCursor(0, 1);
    lcd.print("Soil:"); lcd.print(soilMoisture); lcd.print("%");
    lcd.setCursor(0, 2);
    lcd.print("Water:"); lcd.print(waterLevel); lcd.print("%");
    lcd.setCursor(0, 3);
    lcd.print("System Online");
  }
}

void readAndPublishSensors() {
  temp = dht.readTemperature();
  hum = dht.readHumidity();
  int soilAnalog = analogRead(SOIL_PIN);
  // Map giá trị cho ESP32 (0-4095)
  soilMoisture = map(soilAnalog, 4095, 0, 0, 100); 
  waterLevel = map(readDistanceCm(), 0, 400, 100, 0); // Giả lập mực nước dựa trên khoảng cách

  StaticJsonDocument<256> dataDoc;
  dataDoc["soil"] = soilMoisture;
  dataDoc["water"] = waterLevel;
  if (!isnan(temp)) dataDoc["temp"] = temp;
  if (!isnan(hum)) dataDoc["hum"] = hum;

  char buffer[256];
  serializeJson(dataDoc, buffer);
  mqttClient.publish(TOPIC_SENSOR_DATA, buffer);

  // Xử lý báo lỗi
  StaticJsonDocument<200> errDoc;
  JsonArray error_sensors = errDoc.createNestedArray("sensors");
  if (temp > auto_temp_max) error_sensors.add("high_temp");
  if (soilMoisture < auto_soil_min) error_sensors.add("dry_soil");

  if (error_sensors.size() > 0) {
    errDoc["type"] = "SENSOR_FAULT";
    char errBuffer[256];
    serializeJson(errDoc, errBuffer);
    mqttClient.publish(TOPIC_ALERT_FAULT, errBuffer);
  }
}

// --- 4. Setup & Loop ---
void setup() {
  Serial.begin(9600);
  pinMode(BUZZER_PIN, OUTPUT);
  pinMode(SOIL_PIN, INPUT);
  pinMode(TRIG_PIN, OUTPUT);
  pinMode(ECHO_PIN, INPUT);
  
  dht.begin();
  lcd.init();
  lcd.backlight();
  lcd.print("Booting...");

  setup_wifi();
  mqttClient.setServer(MQTT_SERVER, MQTT_PORT);
  mqttClient.setCallback(callback);
  
  lcd.clear();
  lcd.noBacklight();
}

void loop() {
  if (!mqttClient.connected()) {
    reconnect();
  }
  mqttClient.loop();
  
  controlAndDisplayLCD();

  unsigned long now = millis();
  if (now - lastMsgTime > interval) {
    lastMsgTime = now;
    readAndPublishSensors();
  }
}
