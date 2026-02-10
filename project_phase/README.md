# Phase 1: AI Visual Machine Identification

## 📌 Project Overview: The "Industrial Doctor"
The **Industrial Doctor** is a multimodal diagnostic tool that combines Computer Vision (CV) and Acoustic Analysis to monitor machine health. 

**Phase 1** is the initialization stage. Before the microphone begins "listening," the system uses its "eyes" (Camera) to identify the machine's mechanical parameters. This ensures the diagnosis is tailored specifically to the machine's physics, allowing for accurate monitoring even in loud factory environments.

---

## 🛠 Strategic Goal
The primary objective of Phase 1 is to automate the extraction of the **Machine Signature**. By identifying components visually, the system can:
* **Filter Ambient Noise:** Ignore irrelevant factory sounds that do not correlate to the machine's specific RPM.
* **Auto-Calibration:** Automatically set frequency "Watch Zones" and alarm thresholds.
* **Universal Application:** Switch between a fan, motor, or gearbox without manual code changes.

---

## 🔍 Data Acquisition Logic
Using the **ESP32-S3** and an **OV2640 camera**, the system captures and processes the following "Golden Variables":

### 1. RPM Extraction (Fundamental Frequency $1X$)
* **Method:** Optical Character Recognition (OCR) of the motor nameplate.
* **Purpose:** Establishes the core frequency (e.g., $3600\text{ RPM} = 60\text{Hz}$).

### 2. Component Inventory
* **Blade/Tooth Counting:** Detects the number of fan blades or gear teeth.
* **Purpose:** Calculates "Pass Frequencies" (e.g., 4 blades @ $60\text{Hz}$ = $240\text{Hz}$ Blade Pass Frequency).

### 3. Bearing Identification
* **Method:** Reading model numbers (e.g., "SKF 6205") via AI.
* **Purpose:** References internal geometry to monitor specific ball-pass failure tones.



---

## 🤖 AI Implementation Tiers
The **8MB PSRAM** on the ESP32-S3 is utilized for image buffering and AI inference:

| Tier | Processing Method | Best For |
| :--- | :--- | :--- |
| **Edge AI** | On-device ESP-DL / TensorFlow Lite | High-security zones; No internet access. |
| **Cloud AI** | Vision API (GPT-4o / Custom Server) | Complex, dirty, or damaged nameplates. |

---

## 🚦 The Phase 1 to Phase 2 Transition
Phase 1 is complete when the system triggers a **"Lock-In"** signal:

1. **Metadata Storage:** Identified machine data is loaded into the **PSRAM**.
2. **Frequency Mapping:** The FFT algorithm generates a map of "Expected Vitals."
3. **Phase 2 Trigger:** The system activates the **INMP441 Microphone** and begins acoustic analysis.



---

## 📋 Hardware Requirements (Phase 1)
* **Microcontroller:** ESP32-S3 N16R8.
* **Memory:** 8MB Octal PSRAM (Required for image processing).
* **Camera:** OV2640 Module.
* **Mounting:** Magnet-based coupling for structure-borne vibration transfer.
