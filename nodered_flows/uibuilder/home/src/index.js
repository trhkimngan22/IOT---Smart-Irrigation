/// <reference path="../types/uibuilder.d.ts" />
"use strict";

// CẤU HÌNH DOM 
const btnAuto = document.getElementById("btn-auto");
const btnManual = document.getElementById("btn-manual");
const autoPanel = document.getElementById("auto-panel");
const manualPanel = document.getElementById("manual-panel");
const statusText = document.getElementById("manual-status-text");
const STORE_KEY_MODE = "plant_mode"; // Lưu cục bộ chế độ Auto/Manual
const STORE_KEY_PUMP = "plant_pump_state"; // Lưu cục bộ trạng thái bơm

// CHẠY KHI TRANG LOAD
document.addEventListener("DOMContentLoaded", () => {
  console.log("Web App Loaded - Version: LocalStorage Fix");
  // Đọc biến cục bộ và hiển thị ngay
  restoreFromLocalStorage();
  // Sau đó mới kết nối Node-RED để đồng bộ ngầm
  uibuilder.start({ 'serverPath': '/home' });
  uibuilder.send({ topic: "get_settings" });
  // Lắng nghe phản hồi từ Server
  uibuilder.onChange('msg', msg => {
    if (!msg || !msg.payload) return;
    if (msg.topic === "current_settings") {
      if (msg.payload.settings) updateInputValues(msg.payload.settings);
      if (msg.payload.mode) {
        const serverMode = msg.payload.mode;
        localStorage.setItem(STORE_KEY_MODE, serverMode);
        if (serverMode === "manual") {
          setManualTab();
        } else {
          setAutoTab();
        }
        console.log(`[SERVER SYNC] Mode set to: ${serverMode}`);
      }
      
      // Cập nhật trạng thái bơm từ server
      if (msg.payload.pump !== undefined) {
        const serverPump = msg.payload.pump; // 'on' hoặc 'off'
        localStorage.setItem(STORE_KEY_PUMP, serverPump);
        if (statusText) {
          if (serverPump === "on") {
            statusText.innerText = "Watering...";
            statusText.style.cssText = "color: #2ecc71 !important; font-weight: bold;";
          } else {
            statusText.innerText = "Stopped";
            statusText.style.cssText = "color: #666 !important; font-weight: bold;";
          }
        }
        console.log(`[SERVER SYNC] Pump state set to: ${serverPump}`);
      }
    }

    if (msg.payload.type === "sensors") updateSensorUI(msg.payload.data);
    if (msg.payload.type === "alert") updateStateUI(msg.payload.data);
  });
});

// HÀM KHÔI PHỤC TỪ BIẾN CỤC BỘ 
function restoreFromLocalStorage() {
  const savedMode = localStorage.getItem(STORE_KEY_MODE) || "auto";
  const savedPump = localStorage.getItem(STORE_KEY_PUMP) || "off";
  console.log(`Khôi phục UI từ Local -> Mode: ${savedMode} | Pump: ${savedPump}`);
  // Set Tab
  if (savedMode === "manual") {
    setManualTab();
  } else {
    setAutoTab();
  }
  // Set Status Text 
  if (statusText) {
    if (savedPump === "on") {
      statusText.innerText = "Watering...";
      statusText.style.cssText = "color: #2ecc71 !important; font-weight: bold;";
    } else {
      statusText.innerText = "Stopped";
      statusText.style.cssText = "color: #666 !important; font-weight: bold;";
    }
  }
}

// XỬ LÝ NÚT BẤM
// Nút Chuyển Tab
if (btnAuto) btnAuto.onclick = () => {
  localStorage.setItem(STORE_KEY_MODE, "auto");
  //setManualTab(); 
  setAutoTab();
  uibuilder.send({ topic: "cmd_mode", payload: "auto" });
};

if (btnManual) btnManual.onclick = () => {
  localStorage.setItem(STORE_KEY_MODE, "manual");
  setManualTab();
  uibuilder.send({ topic: "cmd_mode", payload: "manual" });
};

// Nút Bơm START
const btnPumpOn = document.getElementById('btn-pump-on');
if (btnPumpOn) {
  btnPumpOn.onclick = function () {
    console.log("Click START -> Lưu Local: ON");
    // Lưu biến cục bộ
    localStorage.setItem(STORE_KEY_PUMP, "on");
    // Hiển thị ngay lập tức
    if (statusText) {
      statusText.innerText = "Watering...";
      statusText.style.cssText = "color: #2ecc71 !important; font-weight: bold;";
    }
    // Gửi lệnh
    uibuilder.send({ cmd: "PUMP_ON" });
  };
}
// Nút Bơm STOP
const btnPumpOff = document.getElementById('btn-pump-off');
if (btnPumpOff) {
  btnPumpOff.onclick = function () {
    console.log("Click STOP -> Lưu Local: OFF");
    // Lưu biến cục bộ
    localStorage.setItem(STORE_KEY_PUMP, "off");
    // Hiển thị ngay lập tức
    if (statusText) {
      statusText.innerText = "Stopped";
      statusText.style.cssText = "color: #666 !important; font-weight: bold;";
    }
    // Gửi lệnh
    uibuilder.send({ cmd: "PUMP_OFF" });
  };
}
//CÁC HÀM HỖ TRỢ 
function setAutoTab() {
  if (btnAuto) btnAuto.classList.add("active");
  if (btnManual) btnManual.classList.remove("active");
  if (autoPanel) autoPanel.classList.remove("hidden");
  if (manualPanel) manualPanel.classList.add("hidden");
}

function setManualTab() {
  if (btnManual) btnManual.classList.add("active");
  if (btnAuto) btnAuto.classList.remove("active");
  if (manualPanel) manualPanel.classList.remove("hidden");
  if (autoPanel) autoPanel.classList.add("hidden");
}

function updateInputValues(settings) { // Cập nhật giá trị ô nhập liệu
  if (document.getElementById("soil-min-input")) document.getElementById("soil-min-input").value = settings.soilLimit;
  if (document.getElementById("hum-min-input")) document.getElementById("hum-min-input").value = settings.humLimit;
  if (document.getElementById("temp-max-input")) document.getElementById("temp-max-input").value = settings.tempLimit;
}

function updateSensorUI(data) {
  if (document.getElementById("soil-sensor-value")) document.getElementById("soil-sensor-value").innerText = data.soil + "%";
  if (document.getElementById("temp-sensor-value")) document.getElementById("temp-sensor-value").innerText = data.temp + "°C";
  if (document.getElementById("hum-sensor-value")) document.getElementById("hum-sensor-value").innerText = data.hum + "%";
  if (document.getElementById("water-sensor-value")) document.getElementById("water-sensor-value").innerText = data.water + "L";
}

function updateStateUI(data) {
  setStatus("air", data.air);
  setStatus("soil", data.soil);
  setStatus("water", data.water);
}

function setStatus(sensor, state) {
  const dot = document.getElementById(sensor + "-dot");
  if (!dot) return;
  dot.classList.remove("normal", "error");
  if (state === "normal") dot.classList.add("normal");
  else if (state === "fault") dot.classList.add("error");
}

// Xử lý nút Change Settings
document.querySelectorAll('.primary-btn').forEach(btn => {
  btn.onclick = function (e) {
    if (btn.textContent.includes("Change settings")) {
      let soilVal = Number(document.getElementById('soil-min-input').value) || 30;
      let humVal = Number(document.getElementById('hum-min-input').value) || 40;
      let tempVal = Number(document.getElementById('temp-max-input').value) || 35;
      uibuilder.send({ topic: "cmd_settings", payload: { soilLimit: soilVal, humLimit: humVal, tempLimit: tempVal } });
      alert(`Settings saved!`);
    }
  };
});