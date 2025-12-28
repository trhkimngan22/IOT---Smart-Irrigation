#include <Arduino.h>
#include <WiFi.h> 
#include <PubSubClient.h>
#include <ArduinoJson.h> 
#include "DHT.h"           
#include <Adafruit_Sensor.h> 
#include <LiquidCrystal_I2C.h>

// --- Cấu hình WiFi & MQTT ---
const char* WIFI_SSID = "Wokwi-GUEST";
const char* WIFI_PASS = "";

const char* MQTT_SERVER = "broker.hivemq.com";
const int MQTT_PORT = 1883;

// --- Định nghĩa Chân (PIN) cho ESP8266 ---
#define BUZZER_PIN  15 
#define SOIL_PIN    32  
#define WATER_PIN 34
#define TRIG_PIN 26
#define ECHO_PIN 25
#define RELAY_PIN 12

#define DHT_PIN 16 // Chân GPIO 16 được sử dụng để kết nối cảm biến DHT
#define LCD_ADDR 0x27 // Cấu hình LCD I2C
#define LCD_COLS 20     
#define LCD_ROWS 4      
#define MAX_DISTANCE_CM 50 // Ngưỡng khoảng cách (nếu có người đến gần 50cm, màn hình bật)

// --- Định nghĩa MQTT Topics ---
#define TOPIC_SENSOR_DATA   "irrigation/sensors"   // Gửi data (Độ ẩm, nước, nhiệt độ...)
#define TOPIC_CONTROL_PUMP  "irrigation/control"   // Nhận lệnh tưới
#define TOPIC_ALERT_FAULT   "irrigation/alert"     // Gửi báo lỗi cảm biến
DHT dht(DHT_PIN, DHT22); 
LiquidCrystal_I2C lcd(LCD_ADDR, LCD_COLS, LCD_ROWS);

bool isLcdBacklightOn = false; 

// --- Ngưỡng tiêu chuẩn ---
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
  String message = "";
  for (int i = 0; i < length; i++) {
    message += (char)payload[i];
  }
  Serial.print("Message arrived [");
  Serial.print(TOPIC_CONTROL_PUMP);
  Serial.print("]: ");
  Serial.println(message);
  if (String(topic) == TOPIC_CONTROL_PUMP) {
    if (message.indexOf("ON") >= 0) {
      Serial.println(">> COMMAND: TURN PUMP ON");
      digitalWrite(RELAY_PIN, HIGH); 
      digitalWrite(BUZZER_PIN, HIGH);
      delay(100); 
      digitalWrite(BUZZER_PIN, LOW);
    }
    else if (message.indexOf("OFF") >= 0) {
      Serial.println(">> COMMAND: TURN PUMP OFF");
      digitalWrite(RELAY_PIN, LOW);
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
  int soilsensor = analogRead(SOIL_PIN);
  int watersensor = analogRead(WATER_PIN);
  // Map giá trị cho ESP32 (0-4095)
  soilMoisture = map(soilsensor, 1680, 3620, 0, 100); 
  waterLevel = map(watersensor, 0, 4095, 0, 100);

  StaticJsonDocument<256> dataDoc;
  dataDoc["soil"] = soilMoisture;
  dataDoc["water"] = waterLevel;
  if (!isnan(temp)) dataDoc["temp"] = temp;
  if (!isnan(hum)) dataDoc["hum"] = hum;

  char buffer[256];
  serializeJson(dataDoc, buffer);
  mqttClient.publish(TOPIC_SENSOR_DATA, buffer);

  // In dữ liệu sensor ra Serial để kiểm tra
  Serial.print("SENSOR DATA: ");
  Serial.println(buffer);
}

// --- 4. Setup & Loop ---
void setup() {
  Serial.begin(9600);
  pinMode(BUZZER_PIN, OUTPUT);
  pinMode(SOIL_PIN, INPUT);
  pinMode(TRIG_PIN, OUTPUT);
  pinMode(ECHO_PIN, INPUT);
  pinMode(RELAY_PIN, OUTPUT);
  digitalWrite(RELAY_PIN, LOW);
  
  dht.begin();
  lcd.init();
  lcd.backlight();
  lcd.print("Booting...");

  setup_wifi();
  mqttClient.setServer(MQTT_SERVER, MQTT_PORT);
  mqttClient.setCallback(callback);
  
  lcd.clear();
  lcd.noBacklight();
  // resolver.AddUnidirectionalSequenceLSTM(); 
  // resolver.AddFullyConnected();
  // resolver.AddReshape();
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
