#include <Arduino.h>
#include <WiFi.h> // Nếu dùng ESP32 thì đổi thành <WiFi.h>
#include <PubSubClient.h>
#include <ArduinoJson.h> // Thư viện quan trọng để xử lý JSON
#include "config.h"
#include "DHT.h"           // Thư viện DHT sensor
#include <Adafruit_Sensor.h> // Thư viện Adafruit Sensor (DHT cần)
#include <LiquidCrystal_I2C.h>

// Định nghĩa chân cắm
#define DHTPIN 16           
#define DHTTYPE DHT22       
DHT dht(DHTPIN, DHTTYPE);

// Khai báo LCD 
LiquidCrystal_I2C lcd(LCD_ADDR, LCD_COLS, LCD_ROWS);

bool isLcdBacklightOn = false; 
#define MAX_DISTANCE_CM 50

// --- 4. Ngưỡng tiêu chuẩn ---
int auto_soil_min = 30; 
int auto_temp_max = 80;     
int auto_hum_min = 0;       

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

    // Lấy trường 'type' để xác định loại lệnh
    const char* command_type = doc["type"] | "UNKNOWN";

    // 2. Xử lý lệnh SETTING_CHANGE (Đặt ngưỡng tự động)
    if (strcmp(command_type, "SETTING_CHANGE") == 0) {
      auto_soil_min = doc["data"]["soil_min"] | 30;
      auto_temp_max = doc["data"]["temp_max"] | 35;
      auto_hum_min = doc["data"]["hum_min"] | 50;

      Serial.println(">> Updated Auto Watering Settings:");
      Serial.printf("   Soil Min: %d%%, Temp Max: %dC, Hum Min: %d%%\n", 
                    auto_soil_min, auto_temp_max, auto_hum_min);

    } 
    // 3. Xử lý lệnh WATERING
    else if (strcmp(command_type, "WATERING") == 0) {
      int duration = doc["duration_s"] | 0; // Lấy thời gian tưới (s)
      
      if (duration > 0 && duration <= 60) { // Giới hạn tưới thủ công tối đa 60s
        
        Serial.printf(">> MANUAL WATERING started for %d seconds...\n", duration);
        
        // Bật Bơm và Kêu Còi
        digitalWrite(BUZZER_PIN, HIGH); // Bật còi báo hiệu
        // digitalWrite(LED_PIN, HIGH);    // Bật bơm (LED)
        delay(500);                     // Kêu bíp 0.5s
        digitalWrite(BUZZER_PIN, LOW);   // Tắt còi

        delay(duration * 1000); 

        // Tắt Bơm
        // digitalWrite(LED_PIN, LOW);
        Serial.println(">> MANUAL WATERING finished.");
      } else {
         Serial.println(">> Invalid Watering Duration!");
      }
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

// Đọc khoảng cách từ HC-SR04 
long readDistanceCm() {
  digitalWrite(TRIG_PIN, LOW);
  delayMicroseconds(2);
  digitalWrite(TRIG_PIN, HIGH);
  delayMicroseconds(10);
  digitalWrite(TRIG_PIN, LOW);

  long duration = pulseIn(ECHO_PIN, HIGH);
  // Sử dụng kiểu long cho duration và float cho tính toán. Chuyển kết quả về long.
  long distance = (long)(duration * 0.034 / 2); 
  return distance;
}

// --- Hàm điều khiển và hiển thị LCD (Luồng 4) ---
void controlAndDisplayLCD() {
  long distance = readDistanceCm();
    
  // 1. Logic Điều khiển Đèn nền (Energy Saving)
  if (distance <= MAX_DISTANCE_CM) {
      if (!isLcdBacklightOn) {
          lcd.backlight(); // BẬT đèn nền
          isLcdBacklightOn = true;
          lcd.clear(); // Xóa màn hình khi bật lại
      }
  } else {
      if (isLcdBacklightOn) {
          lcd.noBacklight(); // TẮT đèn nền
          isLcdBacklightOn = false;
      }
      return; // Dừng hàm, không hiển thị gì khi đèn nền tắt
  }

  // 2. Hiển thị Dữ liệu trên LCD 20x4 (Khi đèn nền đang BẬT)
  if (isLcdBacklightOn) {
        
      // Hàng 0: Nhiệt độ & Độ ẩm KK
      lcd.setCursor(0, 0);
      lcd.print("T:");
      lcd.print(temp, 1); 
      lcd.print((char)223); // Ký tự độ (°)
      lcd.print("C  ");
        
      lcd.setCursor(10, 0);
      lcd.print("H:");
      lcd.print(hum, 1);
      lcd.print("%          "); // Dùng space để xóa ký tự cũ

      // Hàng 1: Độ ẩm Đất
      lcd.setCursor(0, 1);
      lcd.print("Soil:");
      lcd.print(soilMoisture);
      lcd.print("%          ");

      // Hàng 2: Mực nước
      lcd.setCursor(0, 2);
      lcd.print("Water:");
      lcd.print(waterLevel);
      lcd.print("%          ");
        
      // Hàng 3: Trạng thái (Tùy chọn hiển thị)
      lcd.setCursor(0, 3);
      lcd.print("Online & OK         "); 
  }
}

// --- Hàm giả lập đọc cảm biến & Detect Lỗi ---
void readAndPublishSensors() {
  // 1. Đọc Nhiệt độ và độ ẩm kk
  temp = dht.readTemperature();
  hum = dht.readHumidity();

  // Đọc Độ ẩm Đất 
  int soilAnalog = analogRead(SOIL_PIN);
  // Ánh xạ (Map) giá trị analog (Giả sử 4095=0%, 1500=100% cho ESP32)
  soilMoisture = map(soilAnalog, 4095, 1500, 0, 100); 
  soilMoisture = constrain(soilMoisture, 0, 100); 

  // Đọc Mực nước 
  waterLevel = 100 - readDistanceCm(); // Sử dụng hàm readDistanceCm() thay vì getDistanceCm()

  // Kiểm tra lỗi đọc DHT trước khi Publish
  if (isnan(temp) || isnan(hum)) {
    Serial.println(">>> ERROR: Failed to read from DHT sensor!");
  }               

  // 2. Đóng gói JSON gửi Web (Data bình thường)
  // Format: {"soil": 65, "water": 80, "temp": 30, "hum": 70}
  StaticJsonDocument<200> doc;
  doc["soil"] = soilMoisture;
  doc["water"] = waterLevel;

  // Chỉ thêm Nhiệt độ và Độ ẩm vào gói tin nếu giá trị hợp lệ
  if (!isnan(temp)) { 
    doc["temp"] = temp;
  }
  if (!isnan(hum)) { 
    doc["hum"] = hum;
  }

  char buffer[256];
  serializeJson(doc, buffer);
  mqttClient.publish(TOPIC_SENSOR_DATA, buffer);
  Serial.println("Published Sensor Data");
  Serial.println(buffer);

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
  
  dht.begin(); // Khởi tạo cảm biến DHT

  lcd.init(); // Khởi tạo LCD
  lcd.backlight(); 
  lcd.print("System Initializing...");
  lcd.setCursor(0, 1);
  lcd.print("Connect to WiFi...");

  setup_wifi();
  mqttClient.setServer(MQTT_SERVER, MQTT_PORT);
  mqttClient.setCallback(callback);

  // Tắt đèn nền sau khi khởi động xong
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
