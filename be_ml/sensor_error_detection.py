import paho.mqtt.client as mqtt
import numpy as np
import json
import joblib
from tensorflow.keras.models import load_model

TOPIC_SENSOR_DATA = "irrigation/sensors"
TOPIC_ALERT_FAULT = "irrigation/alert"
BROKER = "broker.hivemq.com"
PORT = 1883

WINDOW_SIZE = 30
N_FEATURES = 4

# --- Load model, scaler, thresholds ---
thresholds = np.load("thresholds.npy", allow_pickle=True).item()
air_model = load_model("air_model.h5", compile=False)
air_scaler = joblib.load("air_scaler.joblib")
soil_model = load_model("soil_model.h5", compile=False)
soil_scaler = joblib.load("soil_scaler.joblib")
water_model = load_model("water_model.h5", compile=False)
water_scaler = joblib.load("water_scaler.joblib")

# --- Sensor window buffer ---
sensor_window = []

# --- Fault counters ---
air_fault_counter = 0
soil_fault_counter = 0
water_fault_counter = 0

def detect_anomaly_logic(data_np):
    air = data_np[:, 0:2]
    soil = data_np[:, 2:3]
    water = data_np[:, 3:]

    air_s = air_scaler.transform(air)
    soil_s = soil_scaler.transform(soil)
    water_s = water_scaler.transform(water)

    air_r = air_s.reshape(1, WINDOW_SIZE, 2)
    soil_r = soil_s.reshape(1, WINDOW_SIZE, 1)
    water_r = water_s.reshape(1, WINDOW_SIZE, 1)

    air_pred = air_model.predict(air_r, verbose=0)
    soil_pred = soil_model.predict(soil_r, verbose=0)
    water_pred = water_model.predict(water_r, verbose=0)

    air_mae = np.mean(np.abs(air_pred - air_r))
    soil_mae = np.mean(np.abs(soil_pred - soil_r))
    water_mae = np.mean(np.abs(water_pred - water_r))

    return {
        "air": "fault" if air_mae > thresholds["air"] else "normal",
        "soil": "fault" if soil_mae > thresholds["soil"] else "normal",
        "water": "fault" if water_mae > thresholds["water"] else "normal",
        "status": 200
    }

def on_connect(client, userdata, flags, rc):
    print("Connected with result code", rc)
    client.subscribe(TOPIC_SENSOR_DATA)

def on_message(client, userdata, msg):
    payload = msg.payload.decode()
    print(f"Received: {payload}")

    global air_fault_counter, soil_fault_counter, water_fault_counter

    try:
        data = json.loads(payload)
        # Lấy các trường cần thiết, mặc định 0 nếu thiếu
        temp = float(data.get("temp", 0))
        hum = float(data.get("hum", 0))
        soil = float(data.get("soil", 0))
        water = float(data.get("water", 0))
        sample = [temp, hum, soil, water]
        # Cập nhật sensor_window
        sensor_window.append(sample)
        if len(sensor_window) > WINDOW_SIZE:
            sensor_window.pop(0)
        # Khi đủ 30 mẫu thì chạy phát hiện lỗi
        if len(sensor_window) == WINDOW_SIZE:
            data_np = np.array(sensor_window, dtype=np.float32)
            result = detect_anomaly_logic(data_np)

            # --- Fault logic: cần >=5 lần liên tiếp mới báo lỗi ---
            # AIR
            if result["air"] == "fault":
                air_fault_counter += 1
            else:
                air_fault_counter = 0
            air_status = "fault" if air_fault_counter >= 5 else "normal"

            # SOIL
            if result["soil"] == "fault":
                soil_fault_counter += 1
            else:
                soil_fault_counter = 0
            soil_status = "fault" if soil_fault_counter >= 5 else "normal"

            # WATER
            if result["water"] == "fault":
                water_fault_counter += 1
            else:
                water_fault_counter = 0
            water_status = "fault" if water_fault_counter >= 5 else "normal"

            result_to_send = {
                "air": air_status,
                "soil": soil_status,
                "water": water_status,
                "status": 200
            }
            result_json = json.dumps(result_to_send)
            client.publish(TOPIC_ALERT_FAULT, result_json)
            print(f"Sent anomaly result: {result_json}")
    except Exception as e:
        print(f"Error processing sensor data: {e}")

client = mqtt.Client()
client.on_connect = on_connect
client.on_message = on_message

client.connect(BROKER, PORT, 60)
client.loop_forever()
