/// <reference path="../types/uibuilder.d.ts" />
"use strict";

const btnAuto = document.getElementById("btn-auto");
const btnManual = document.getElementById("btn-manual");
const autoPanel = document.getElementById("auto-panel");
const manualPanel = document.getElementById("manual-panel");

// --- 1. HANDLE MODE SWITCHING ---
btnAuto.onclick = () => {
  btnAuto.classList.add("active");
  btnManual.classList.remove("active");
  autoPanel.classList.remove("hidden");
  manualPanel.classList.add("hidden");
  uibuilder.send({ topic: "cmd_mode", payload: "auto" });
  console.log("Switched to AUTO mode");
};

btnManual.onclick = () => {
  btnManual.classList.add("active");
  btnAuto.classList.remove("active");
  manualPanel.classList.remove("hidden");
  autoPanel.classList.add("hidden");
  uibuilder.send({ topic: "cmd_mode", payload: "manual" });
  console.log("Switched to MANUAL mode");
};

// --- 2. START UIBUILDER ---
document.addEventListener("DOMContentLoaded", () => {
  uibuilder.start({ 'serverPath': '/home' });
  uibuilder.onChange('msg', msg => {
    if (!msg || !msg.payload) return;
    const { type, data } = msg.payload;
    if (type === "sensors") updateSensorUI(data);
    if (type === "alert") updateStateUI(data);
  });
});

// --- 3. UPDATE UI FUNCTIONS ---
function updateSensorUI(data) {
  if(document.getElementById("soil-sensor-value")) document.getElementById("soil-sensor-value").innerText = data.soil + "%";
  if(document.getElementById("temp-sensor-value")) document.getElementById("temp-sensor-value").innerText = data.temp + "°C";
  if(document.getElementById("hum-sensor-value")) document.getElementById("hum-sensor-value").innerText = data.hum + "%";
  if(document.getElementById("water-sensor-value")) document.getElementById("water-sensor-value").innerText = data.water + "L";
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
  if (state === "ok") dot.classList.add("normal");
  else if (state === "fault") dot.classList.add("error");
}

// --- 4. HANDLE SETTINGS ---
document.querySelectorAll('.primary-btn').forEach(btn => {
  btn.onclick = function (e) {
    if (btn.textContent.includes("Change settings")) {
      let soilVal = Number(document.getElementById('soil-min-input').value) || 30;
      let humVal = Number(document.getElementById('hum-min-input').value) || 40;
      let tempVal = Number(document.getElementById('temp-max-input').value) || 35;

      uibuilder.send({
        topic: "cmd_settings",
        payload: { soilLimit: soilVal, humLimit: humVal, tempLimit: tempVal }
      });
      alert(`Settings saved:\nSoil Min: ${soilVal}%\nHum Min: ${humVal}%\nTemp Max: ${tempVal}°C`);
    } 
  };
});

// --- 5. HANDLE MANUAL PUMP CONTROL (START / STOP) ---
const btnPumpOn = document.getElementById('btn-pump-on');
const btnPumpOff = document.getElementById('btn-pump-off');
const statusText = document.getElementById('manual-status-text');

if (btnPumpOn && btnPumpOff) {
  // START BUTTON
  btnPumpOn.onclick = function() {
    console.log("Sending PUMP_ON command...");
    uibuilder.send({ cmd: "PUMP_ON" });
    if (statusText) {
        statusText.innerText = "Watering...";
        statusText.style.color = "#2ecc71";
    }
  };

  // STOP BUTTON
  btnPumpOff.onclick = function() {
    console.log("Sending PUMP_OFF command...");
    uibuilder.send({ cmd: "PUMP_OFF" });
    if (statusText) {
        statusText.innerText = "Stopped";
        statusText.style.color = "#666";
    }
  };
}