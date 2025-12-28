# Smart Irrigation - ML Sensor Fault Detection

## 1. Tạo môi trường ảo Python (khuyến nghị)

```bash
cd be_ml
python -m venv sensor_env
# Kích hoạt:
# Windows:
sensor_env\Scripts\activate
# Linux/Mac:
# source sensor_env/bin/activate
```

## 2. Cài đặt thư viện cần thiết

```bash
pip install -r requirements.txt
```

## 3. Chuẩn bị mô hình & scaler

- Đảm bảo các file sau có trong thư mục `be_ml`:
  - `air_model.h5`
  - `soil_model.h5`
  - `water_model.h5`
  - `air_scaler.joblib`
  - `soil_scaler.joblib`
  - `water_scaler.joblib`
  - `thresholds.npy`

## 4. Chạy script phát hiện lỗi cảm biến qua MQTT

```bash
python sensor_error_detection.py
```

- Script sẽ tự động lắng nghe dữ liệu từ topic MQTT `irrigation/sensors`, gom 30 mẫu liên tục, phát hiện lỗi bằng mô hình, và gửi kết quả lên topic `irrigation/alert`.

## 5. (Tùy chọn) Chạy API dự đoán qua Gradio

Nếu muốn test nhanh qua giao diện web/API:

```bash
python model_testing.py
```

- Truy cập: http://localhost:7860 để nhập chuỗi JSON 30x4 và nhận kết quả dự đoán.

---

### 6. (Tùy chọn) Train lại mô hình hoặc xem lại mô hình
```bash
python model.py
```
