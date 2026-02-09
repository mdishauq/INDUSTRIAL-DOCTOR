# 📐 PCB Design & Hardware Architecture

This section details the physical engineering of the **Industrial Doctor** standalone board. The design prioritizes acoustic isolation and modularity to ensure professional-grade diagnostic data.

## 🛠️ The "Split-Probe" Solid PCB Design
The system is built on a single, solid FR4 PCB, but the layout is logically and physically partitioned into three distinct zones to eliminate mechanical and electronic noise interference.

### 1. Probe Head (Acoustic Zone)
* **Component:** INMP441 I2S Digital MEMS Microphone.
* **Engineering:** Isolated at the top tip of the board.
* **Mounting:** Placed on high-profile female headers to align the sensor port directly with the 2mm acoustic aperture in the enclosure.

### 2. Isolation Neck (Vibration Buffer)
* **Structure:** A 30mm x 10mm narrow bridge.
* **Function:** Acts as a physical high-pass filter for mechanical vibrations. It ensures that "grip noise" from the user's hand and vibrations from the battery/boost converter do not reach the microphone.

### 3. Main Chassis (Processing & Power Hub)
* **Brain:** ESP32-S3 N16R8 (Mounted on dual female headers).
* **Visuals:** ILI9341 2.4" TFT Display (SPI Interface).
* **Power:** TP4056 Type-C Charging Module + MT3608 5V Boost Converter.
* **Modularity:** Every active component is connected via female headers, allowing for rapid hardware swaps and easy maintenance during industrial field testing.

## ⚡ Layout Specifications
| Parameter | Specification |
| :--- | :--- |
| **PCB Type** | Solid Single-Piece FR4 |
| **Geometry** | "Key" Shape / Metal Detector Style |
| **Signal Integrity** | Star-Grounding for low-noise I2S data |
| **Power Logic** | 3.7V Li-ion ➡️ 5V Boost ➡️ ESP32 LDO ➡️ 3.3V Mic |

---
*Note: This architecture directly supports the project objective of non-contact high-frequency (0-10kHz) spectral analysis.*
