# Industrial Doctor: AI-Powered Acoustic Diagnostic System 🩺🏭
> **A standalone handheld device for non-contact predictive maintenance using Edge AI and Spectral Analysis.**

## 📌 Project Overview
The **Industrial Doctor** is an embedded AI solution designed to detect mechanical faults in industrial machinery through acoustic "Signature Mapping". By capturing high-frequency sound waves (0-10kHz) with a digital MEMS microphone, the system performs real-time **FFT (Fast Fourier Transform)** and uses a TinyML model to categorize the machine's health into three zones:
* 🔴 **Red Zone:** Immediate failure risk / Critical fault.
* 🟡 **Yellow Zone:** Maintenance required / Early degradation.
* 🔵 **Blue Zone:** Normal operating condition.

## 🛠️ Tech Stack
* **Microcontroller:** ESP32-S3 (N16R8) with 8MB PSRAM for high-speed AI inference.
* **Acoustic Sensor:** INMP441 I2S Digital Microphone.
* **Display:** ILI9341 2.4" TFT LCD for real-time visual feedback.
* **AI Framework:** Edge Impulse (TinyML) for anomaly detection.
* **Power:** HW-183 Dual 18650 Battery Shield + TP4056 Type-C Charging.

## 📐 Hardware Architecture
The device is a standalone handheld probe. The PCB is modular, using female header pins to hold the ESP32-S3 for easy serviceability.

## 📊 Methodology
1. **Data Acquisition:** Sound samples collected via I2S microphone.
2. **Feature Extraction:** Real-time FFT converts time-domain audio to frequency-domain.
3. **Inference:** On-device AI model classifies the spectral data.
4. **Visualization:** Health status mapped onto the "Signature Map" on the TFT screen.

## 👨‍💻 Team
* **MOHAMED ISHAUQ** - Embedded firmware/AI Developer
* **ABDUL RAZZACK** - Hardware Design
* **SHABAZ KHAN** - APP development/3D Modeling
* **MOHAMMED ASHIK** - Project Showcase Website
* **Project Supervisor:** [GURUJI NAME IS UNKOWN]
