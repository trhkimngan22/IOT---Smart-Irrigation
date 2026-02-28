# Hệ thống Tưới Cây Tự Động Thông Minh (IoT Smart Irrigation)

## 📋 Giới thiệu

Hệ thống tưới cây tự động thông minh sử dụng IoT và Machine Learning để:
- Giám sát các thông số môi trường (độ ẩm đất, nhiệt độ, độ ẩm không khí, mực nước).
- Tự động điều khiển máy bơm tưới dựa trên ngưỡng đã cài đặt.
- Phát hiện lỗi cảm biến thông qua mô hình Machine Learning.
- Hiển thị dữ liệu và điều khiển qua giao diện web dashboard.

## 🏗️ Kiến trúc hệ thống

```
┌─────────────────┐      MQTT        ┌──────────────────┐
│   ESP32 + Sensors│ ────────────────▶│   Node-RED       │
│   (Firmware)     │◀──────────────── │   (IoT Gateway)  │
└─────────────────┘                   └──────────────────┘
                                            │
                                            ├──▶ MongoDB
                                            ├──▶ UI Dashboard
                                            │
                                            ▼
                                      ┌──────────────────┐
                                      │  ML Backend      │
                                      │  (Fault Detection)│
                                      └──────────────────┘
```

### Các thành phần chính:

1. **Firmware (ESP32)** - Thu thập dữ liệu và điều khiển thiết bị
   - Cảm biến DHT22 (nhiệt độ, độ ẩm không khí).
   - Cảm biến độ ẩm đất.
   - Cảm biến mực nước.
   - Relay điều khiển máy bơm.
   - LCD hiển thị thông tin.
   - Kết nối MQTT.

2. **Backend ML (Python)** - Phát hiện lỗi cảm biến.
   - 3 mô hình LSTM (air, soil, water).
   - Phân tích chuỗi thời gian 30 mẫu.
   - Gửi cảnh báo qua MQTT.

3. **Node-RED** - Trung tâm xử lý và giao diện.
   - Nhận dữ liệu từ ESP32.
   - Lưu trữ vào MongoDB.
   - Dashboard điều khiển và giám sát.
   - Gửi thông báo qua email và push notification.

## 🚀 Cài đặt và chạy

### 1. Firmware (ESP32)

**Yêu cầu:**
- PlatformIO.
- ESP32 board.
- Các thư viện: WiFi, PubSubClient, ArduinoJson, DHT, LiquidCrystal_I2C.

**Cài đặt:**
```bash
cd firmware/irrigation
# Nếu dùng PlatformIO:
pio run --target upload
```

**Cấu hình WiFi & MQTT:**
Sửa trong file [main.ino](firmware/irrigation/src/main.ino):
```cpp
const char* WIFI_SSID = "your_wifi_name";
const char* WIFI_PASS = "your_password";
const char* MQTT_SERVER = "broker.hivemq.com"; // Hoặc broker của bạn
```

**Sơ đồ kết nối:**
- DHT22: GPIO 16.
- Soil Sensor: GPIO 32.
- Water Level: GPIO 34.
- Relay (Pump): GPIO 12.
- Buzzer: GPIO 15.
- LCD I2C: SDA/SCL (0x27).
- Ultrasonic: TRIG-26, ECHO-25.

### 2. Backend ML (Phát hiện lỗi cảm biến)

**Yêu cầu:**
- Python 3.8+
- Các mô hình đã được train (*.h5, *.joblib)

**Cài đặt:**
```bash
cd be_ml

# Tạo môi trường ảo (khuyến nghị)
python -m venv sensor_env

# Kích hoạt môi trường
# Windows: sensor_env\Scripts\activate
# macOS/Linux: source sensor_env/bin/activate

# Cài đặt dependencies
pip install -r requirements.txt
```

**Chạy service phát hiện lỗi:**
```bash
python sensor_error_detection.py
```

Service sẽ:
- Lắng nghe MQTT topic `irrigation/sensors`.
- Gom 30 mẫu liên tục.
- Phát hiện lỗi bằng mô hình LSTM.
- Gửi cảnh báo lên topic `irrigation/alert`.

**Test mô hình qua Gradio (tùy chọn):**
```bash
python model_testing.py
# Truy cập: http://localhost:7860
```

Chi tiết xem thêm tại [be_ml/README.md](be_ml/README.md)

### 3. Node-RED (IoT Gateway & Dashboard)

**Yêu cầu:**
- Node.js 14+
- Node-RED

**Cài đặt:**
```bash
cd nodered_flows

# Cài đặt Node-RED global (nếu chưa có)
npm install -g node-red

# Cài đặt các node dependencies
npm install
```

**Chạy Node-RED:**
```bash
node-red
```

**Import flows:**
1. Truy cập http://localhost:1880.
2. Menu ≡ → Import → Clipboard.
3. Copy nội dung từ [flows.json](nodered_flows/flows.json).
4. Click Import.

**Cấu hình:**
- MQTT broker: Sửa node MQTT trong flow.
- MongoDB: Cấu hình connection string.
- Email/Push notification: Cấu hình credentials.

**Truy cập Dashboard:**
- Trang chủ: http://localhost:1880/home
- Dashboard chính: http://localhost:1880/dashboard

## 📊 MQTT Topics

| Topic | Direction | Description |
|-------|-----------|-------------|
| `irrigation/sensors` | ESP32 → Server | Dữ liệu cảm biến (JSON) |
| `irrigation/control` | Server → ESP32 | Lệnh điều khiển máy bơm (ON/OFF) |
| `irrigation/alert` | ML → Server | Cảnh báo lỗi cảm biến |

**Format dữ liệu cảm biến:**
```json
{
  "soil_moisture": 45.2,
  "water_level": 78.5,
  "temperature": 28.3,
  "humidity": 65.0,
  "timestamp": 1709136000
}
```

## 🔧 Tính năng

### Firmware
- Đọc dữ liệu từ 4 loại cảm biến.
- Gửi dữ liệu qua MQTT mỗi 5 giây.
- Nhận lệnh điều khiển máy bơm.
- Tự động tưới dựa trên ngưỡng.
- Hiển thị trạng thái trên LCD.
- Cảnh báo bằng buzzer.
- Tự động bật/tắt LCD khi có người gần (ultrasonic).

### ML Backend
- Phát hiện lỗi cảm biến qua mô hình LSTM.
- Xử lý 3 loại dữ liệu: Air (temp, hum), Soil, Water.
- Phân tích chuỗi thời gian 30 mẫu.
- Tính điểm bất thường (anomaly score).
- Gửi cảnh báo tự động qua MQTT.

### Node-RED Dashboard
- Giám sát real-time các thông số.
- Biểu đồ lịch sử dữ liệu.
- Điều khiển máy bơm thủ công.
- Cài đặt ngưỡng tưới tự động.
- Nhận thông báo lỗi cảm biến.
- Lưu trữ dữ liệu vào MongoDB.
- Gửi email/push notification khi có cảnh báo.

## 📁 Cấu trúc thư mục

```
IOT---Smart-Irrigation/
├── README.md                  # File này
├── firmware/                  # Code ESP32
│   └── irrigation/
│       ├── platformio.ini     # Cấu hình PlatformIO
│       └── src/
│           └── main.ino       # Code chính
├── be_ml/                     # Backend Machine Learning
│   ├── sensor_error_detection.py  # Script chính
│   ├── model_testing.py       # Test với Gradio
│   ├── *_model.h5            # Mô hình LSTM đã train
│   ├── *_scaler.joblib       # Scalers
│   ├── thresholds.npy        # Ngưỡng bất thường
│   ├── requirements.txt      # Python dependencies
│   └── README.md             # Hướng dẫn chi tiết
└── nodered_flows/            # Node-RED
    ├── flows.json            # Flow chính
    ├── package.json          # Node dependencies
    └── uibuilder/            # Custom UI
        ├── dashboard/
        └── home/
```

## 🔐 Bảo mật

- Đổi MQTT broker mặc định nếu deploy production.
- Sử dụng MQTT với authentication (username/password).
- Bật TLS/SSL cho MQTT connection.
- Đặt password cho MongoDB.
- Không commit credentials vào Git.

## 🛠️ Troubleshooting

### ESP32 không kết nối WiFi
- Kiểm tra SSID/password.
- Đảm bảo ESP32 trong vùng phủ sóng.
- Thử reset ESP32.

### MQTT không kết nối
- Kiểm tra broker address.
- Ping broker để test network.
- Kiểm tra firewall.

### ML không phát hiện lỗi
- Đảm bảo đã có đủ 30 mẫu dữ liệu.
- Kiểm tra format JSON từ ESP32.
- Xem log trong terminal.

### Node-RED không nhận dữ liệu
- Kiểm tra MQTT node đã kết nối.
- Debug bằng node Debug.
- Kiểm tra topic name.

## 📝 License

MIT License - Tự do sử dụng và chỉnh sửa

## 👥 Đóng góp

Mọi đóng góp đều được hoan nghênh! Hãy tạo Pull Request hoặc Issue.

--- 
**Năm:** 2025