# DeeThunder RC Car — "Radio-Silent" Cockpit

**High-performance, Bluetooth-Exclusive firmware for the ESP32 Classic.**  
This firmware implements an "Exclusive Radio Mode"—WiFi is completely disabled to give the PS4 DualShock 4 controller 100% of the antenna power, ensuring zero latency and zero disconnects. 

Telemetry is provided via an on-board **OLED Display** and **Haptic/LED Feedback** on the controller itself.

---

## 🏎️ Telemetry Guide

### 1. Physical Dashboard (OLED)
The 1.3" OLED (SH1106/SSD1306) on the car provides a real-time cockpit view:
- **Heading:** Gear indicator (G1-G3) and Connection state.
- **Center:** Large digital Speedometer (km/h).
- **Footer:** Battery Voltage (V) and Tilt angles (Roll/Pitch).

### 2. Sensory Feedback (Light Bar & Rumble)
The PS4 Controller acts as a sensory telemetry device, keeping your hands on the sticks and eyes on the track:
- **Light Bar (Battery):** Green (>11.5V) → Yellow → Red (<10.5V) → **Flashing Red** (Critical Warning).
- **Impact Rumble:** The controller vibrates hard if the onboard IMU detects a high-G collision (>3.5G).
- **Gear Rumble:** A short "haptic thud" confirms every gear shift (Triangle/Square).

---

## 🎮 PS4 Controller Guide

| Button | Action | Notes |
| :--- | :--- | :--- |
| **Left Stick** | **Arcade Drive** | Single-stick speed and differential steering |
| **Triangle / Square** | **Gear Change** | Cycle through Gear 1 (35%), 2 (70%), or 3 (100%) |
| **Cross (X)** | **E-Stop** | Kills all motors and clears all rumble/LEDs |
| **Circle / Options** | **Mode Toggle** | Switches between **Arcade** and **Tank** Drive |

---

## 🔌 Hardware Mapping (ESP32 Classic)

| Component | Function | GPIO | Notes |
| :--- | :--- | :--- | :--- |
| **Motors (L)** | PWM / IN1-2 | **25 / 26, 27** | L298N Channel A |
| **Motors (R)** | PWM / IN3-4 | **14 / 32, 33** | L298N Channel B |
| **OLED / IMU** | SDA / SCL | **21 / 22** | Shared Hardware I2C |
| **GPS** | RX / TX | **16 / 17** | Hardware UART1 |
| **Battery** | ADC | **34** | ADC1 (Radio Safe) |

---

## 🛠️ Deploying "Radio-Silent" Mode

1.  **CPU Speed**: The system runs at **240MHz** to ensure zero-latency OLED rendering and PS4 HID processing.
2.  **Flash**:
    ```bash
    pio run -t upload -t monitor
    ```

*Performance optimized by DeeThunder Nexus Ventures.*
