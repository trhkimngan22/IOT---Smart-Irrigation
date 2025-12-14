#ifndef CONFIG_H
#define CONFIG_H

// --- 1. Cấu hình WiFi & MQTT ---
const char* WIFI_SSID = "Wokwi-GUEST";
const char* WIFI_PASS = "";

const char* MQTT_SERVER = "broker.hivemq.com";
const int MQTT_PORT = 1883;

// --- 2. Định nghĩa Chân (PIN) cho ESP8266 ---
// Lưu ý sau: Wokwi dùng ESP32 thì giữ nguyên số, ESP8266 thì D1, D2...
#define BUZZER_PIN  15 
#define SOIL_PIN    34  
#define TRIG_PIN 26
#define ECHO_PIN 25

// --- 3. MQTT Topics (Nên gom nhóm) ---
// Thay 22120421 bằng ID của bạn
#define TOPIC_SENSOR_DATA   "irrigation/sensors"   // Gửi data (Độ ẩm, nước, nhiệt độ...)
#define TOPIC_CONTROL_PUMP  "irrigation/control"   // Nhận lệnh tưới
#define TOPIC_ALERT_FAULT   "irrigation/alert"     // Gửi báo lỗi cảm biến


#endif