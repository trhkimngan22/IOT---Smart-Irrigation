#include <Arduino.h>
#include <WiFi.h> // Nếu dùng ESP32 thì đổi thành <WiFi.h>
#include <PubSubClient.h>
#include <ArduinoJson.h> // Thư viện quan trọng để xử lý JSON
#include "config.h"
      

WiFiClient wifiClient;
PubSubClient mqttClient(wifiClient);

// Biến quản lý thời gian
unsigned long lastMsgTime = 0;
const long interval = 5000; // Gửi dữ liệu mỗi 5s

// Biến lưu trạng thái giả lập
int soilMoisture = 0;
int waterLevel = 0;
float temp = 0;
float hum = 0;

// --- Ngưỡng tiêu chuẩn ---
int auto_soil_min = 30; 
int auto_temp_max = 80;     
int auto_hum_min = 0; 

StaticJsonDocument<512> doc;

// Hàm kết nối WiFi
void setup_wifi() {
  delay(10);
  Serial.println();
  Serial.print("Connecting to ");
  Serial.println(WIFI_SSID);

  WiFi.begin(WIFI_SSID, WIFI_PASS);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nWiFi connected");
}

// --- Hàm xử lý khi nhận lệnh từ Web ---
void callback(char* topic, byte* payload, unsigned int length) {
  Serial.print("Message arrived [");
  Serial.print(topic);
  Serial.print("]: ");

  // 1. Kiểm tra topic là control
  if (String(topic) == TOPIC_CONTROL_PUMP) {

    DeserializationError error = deserializeJson(doc, payload, length);

    if (error) {
      Serial.print(F("deserializeJson() failed: "));
      Serial.println(error.f_str());
      return;
    }

    // Chỉ xử lý msg từ chế độ Auto
    const char* cmd = doc["command"]; // ON || OFF
    const char* type = doc["type"];
    if (strcmp(type, "AUTO") == 0){ //AUTO
      int soil_max = doc["threshold"]["soil"];
      int temp_max = doc["threshold"]["temp"];
      int hum_min = doc["threshold"]["hum"];
      
      // Cập nhật ngưỡng cho chế độ tự động
      auto_soil_min = doc["threshold"]["soil"];
      auto_temp_max = doc["threshold"]["temp"];
      auto_hum_min = doc["threshold"]["hum"];

      Serial.println(">> Updated Auto Watering Settings:");
      Serial.printf("   Soil Min: %d%%, Temp Max: %dC, Hum Min: %d%%\n", 
                      auto_soil_min, auto_temp_max, auto_hum_min);
      if (strcmp(cmd, "ON") == 0){
        int duration = doc["duration"]; // xử lý trong node-red rồi gửi sang
        Serial.printf(">> AUTO WATERING started for %d seconds...\n", duration);
        
        // Bật Bơm và Kêu Còi
        digitalWrite(BUZZER_PIN, HIGH); // Bật còi báo hiệu
        digitalWrite(RELAY_PIN, HIGH);    // Bật bơm (Relay)
        delay(500);                     // Kêu bíp 0.5s
        digitalWrite(BUZZER_PIN, LOW);   // Tắt còi
        delay(duration * 1000); 
        // Tắt Bơm
        digitalWrite(RELAY_PIN, LOW);
        Serial.println(">> AUTO WATERING finished.");
      }
      else {
        digitalWrite(RELAY_PIN, LOW); // Tắt bơm nếu lệnh OFF
        Serial.println(">> AUTO WATERING stopped.");
      }
    }
    else { //MANUAL
      if (strcmp(cmd, "ON") == 0) digitalWrite(RELAY_PIN, HIGH);
      else digitalWrite(RELAY_PIN, LOW);
    }

  }
}

// --- Hàm kết nối lại MQTT ---
void reconnect() {
  while (!mqttClient.connected()) {
    Serial.print("Attempting MQTT connection...");
    String clientId = "ESP8266Client-" + String(random(0xffff), HEX);
    
    if (mqttClient.connect(clientId.c_str())) {
      Serial.println("connected");
      // Đăng ký nhận lệnh điều khiển
      mqttClient.subscribe(TOPIC_CONTROL_PUMP);
    } else {
      Serial.print("failed, rc=");
      Serial.print(mqttClient.state());
      Serial.println("Try again in 5 seconds");
      delay(5000);
    }
  }
}

float getDistanceCm(){
  digitalWrite(TRIG_PIN, LOW);
  delayMicroseconds(2);
  digitalWrite(TRIG_PIN, HIGH);
  delayMicroseconds(10);
  digitalWrite(TRIG_PIN, LOW);

  long duration = pulseIn(ECHO_PIN, HIGH);
  float distance = duration * 0.034 / 2; 
  return distance;
}

// --- Hàm giả lập đọc cảm biến & Detect Lỗi ---
void readAndPublishSensors() {
  // 1. Giả lập dữ liệu (Random)
  soilMoisture = random(0, 100);      
  waterLevel = 100 - getDistanceCm();  
  temp = random(20, 90);               
  hum = random(-5, 100);               

  // 2. Đóng gói JSON gửi Web (Data bình thường)
  // Format: {"soil": 65, "water": 80, "temp": 30, "hum": 70}
  StaticJsonDocument<200> doc;
  doc["soil"] = soilMoisture;
  doc["water"] = waterLevel;
  doc["temp"] = temp;
  doc["hum"] = hum;

  char buffer[256];
  serializeJson(doc, buffer);
  mqttClient.publish(TOPIC_SENSOR_DATA, buffer);
  Serial.println("Published Sensor Data");

  // Detect Lỗi 
  bool hasError = false;
  StaticJsonDocument<200> errDoc;
  JsonArray error_sensors = errDoc.createNestedArray("sensors");
  // Lỗi Nhiệt độ 
  if (temp > auto_temp_max) {
    error_sensors.add("temperature_humidity"); 
  }
  
  // Lỗi Độ ẩm không khí
  if (hum < auto_hum_min || hum > 100) {
    error_sensors.add("temperature_humidity");
  }
  
  // Lỗi Độ ẩm đất 
  if (soilMoisture == 0 || soilMoisture == 100) { 
     error_sensors.add("soil_moisture");
  }
  
  // Lỗi Mực nước
  if (waterLevel < 0) { 
     error_sensors.add("water_level");
  }

  // --- 2. Publish nếu có lỗi ---
  if (error_sensors.size() > 0) {    
    errDoc["type"] = "SENSOR_FAULT";
    errDoc["timestamp"] = millis();

    char errBuffer[256];
    serializeJson(errDoc, errBuffer);
    mqttClient.publish(TOPIC_ALERT_FAULT, errBuffer);
    Serial.printf("FAULT DETECTED on %d sensors & SENT: %s\n", error_sensors.size(), errBuffer);
  }
  
  // Xóa tài liệu JSON sau khi gửi (hoặc để PIO tự quản lý nếu khai báo local)
  errDoc.clear();

}

void setup() {
  Serial.begin(9600);
  pinMode(BUZZER_PIN, OUTPUT);
  pinMode(SOIL_PIN, INPUT);
  pinMode(TRIG_PIN, OUTPUT);
  pinMode(ECHO_PIN, INPUT);
  pinMode(RELAY_PIN, OUTPUT);
  digitalWrite(RELAY_PIN, LOW);

  setup_wifi();
  mqttClient.setServer(MQTT_SERVER, MQTT_PORT);
  mqttClient.setCallback(callback);
}

void loop() {
  if (!mqttClient.connected()) {
    reconnect();
  }
  mqttClient.loop();

  unsigned long now = millis();
  if (now - lastMsgTime > interval) {
    lastMsgTime = now;
    readAndPublishSensors();
  }
}