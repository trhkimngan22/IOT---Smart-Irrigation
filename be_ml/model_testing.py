import json
import numpy as np
import joblib
import gradio as gr 
from tensorflow.keras.models import load_model

WINDOW_SIZE = 30
N_FEATURES = 4 # [Temp, Humid, Soil, Water]

# LOAD MODELS
try:
    thresholds = np.load("thresholds.npy", allow_pickle=True).item()

    air_model = load_model("air_model.h5", compile=False)
    air_scaler = joblib.load("air_scaler.joblib")

    soil_model = load_model("soil_model.h5", compile=False)
    soil_scaler = joblib.load("soil_scaler.joblib")

    water_model = load_model("water_model.h5", compile=False)
    water_scaler = joblib.load("water_scaler.joblib")
    
    print("Tải mô hình và scaler thành công.")

except FileNotFoundError as e:
    print(f"LỖI TẢI FILE: {e}. Vui lòng kiểm tra các file model/scaler/thresholds.")
    exit()

# LOGIC CHÍNH
def detect_anomaly_logic(json_payload_str: str):
    
    # 1. Phân tích cú pháp JSON String thành Dictionary Python
    try:
        payload_dict = json.loads(json_payload_str)
        raw_data_list = payload_dict.get("raw_data_2d") 
        
        if raw_data_list is None:
            return {"error": "JSON body must contain 'raw_data_2d' key."}
            
    except json.JSONDecodeError:
        return {"error": "Invalid JSON format."}

    # 2. Chuyển List 2D thành NumPy Array
    data_np = np.array(raw_data_list, dtype=np.float32)
    if data_np.shape != (WINDOW_SIZE, N_FEATURES):
        return {
            "error": "Data shape mismatch",
            "detail": f"Expected ({WINDOW_SIZE},{N_FEATURES}), got {data_np.shape}"
        }

    # 3. Trích xuất features
    air = data_np[:, 0:2]   # Cột 0, 1 (Temp, Humid)
    soil = data_np[:, 2:3]  # Cột 2 (Soil Moisture)
    water = data_np[:, 3:]  # Cột 3 (Water Level)

    # 5. Scaling
    air_s = air_scaler.transform(air)
    soil_s = soil_scaler.transform(soil)
    water_s = water_scaler.transform(water)

    # 6. Reshaping cho mô hình (Thêm chiều Batch, Shape: (1, 30, N_features))
    air_r = air_s.reshape(1, WINDOW_SIZE, 2)
    soil_r = soil_s.reshape(1, WINDOW_SIZE, 1)
    water_r = water_s.reshape(1, WINDOW_SIZE, 1)

    # 7. Predict
    air_pred = air_model.predict(air_r, verbose=0)
    soil_pred = soil_model.predict(soil_r, verbose=0)
    water_pred = water_model.predict(water_r, verbose=0)

    # 8. Tính MAE
    air_mae = np.mean(np.abs(air_pred - air_r))
    soil_mae = np.mean(np.abs(soil_pred - soil_r))
    water_mae = np.mean(np.abs(water_pred - water_r))

    # 9. Đánh giá Anomaly
    return {
        "air": "fault" if air_mae > thresholds["air"] else "normal",
        "soil": "fault" if soil_mae > thresholds["soil"] else "normal",
        "water": "fault" if water_mae > thresholds["water"] else "normal",
        "status": 200 # Mã trạng thái 200 (Thành công)
    }

# KHỞI TẠO GRADIO INTERFACE

# Gradio sẽ tự động tạo API POST endpoint tại /api/predict
iface = gr.Interface(
    fn=detect_anomaly_logic,
    
    inputs=gr.Textbox(
        lines=10, 
        placeholder='Dán payload JSON 30x4 vào đây: {"raw_data_2d": [[...]]}', 
        label="JSON Payload (String)"
    ),
    
    # API: Đầu ra là JSON Result
    outputs=gr.JSON(label="Prediction Result"),
    
    title="IoT Anomaly Detection Autoencoder API (Gradio)",
    description="Nhận chuỗi JSON 30x4 từ ESP32 và trả về trạng thái lỗi (fault/normal) cho từng cảm biến."
)

# ---------------- CHẠY GRADIO ----------------
iface.launch(
    server_port=7860, 
    share=False 
)