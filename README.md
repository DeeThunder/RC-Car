# DeeThunder RC Car — Pro Firmware

**High-performance, bare-metal RC car firmware for the ESP32 Classic.**  
Designed for sub-millisecond control latency, real-time telemetry, and rugged stability using a PS4 DualShock 4 controller.

---

## 🎮 PS4 Controller Guide

The car defaults to **Arcade Mode** (One-stick control) on boot.

| Button | Action | Notes |
| :--- | :--- | :--- |
| **Left Stick (Up/Down)** | **Speed** | Forward / Backward movement |
| **Left Stick (Left/Right)** | **Steer** | Differential steering (Arcade Mode) |
| **Circle / Options** | **Mode Toggle** | Switches between **Arcade** and **Tank** Drive |
| **Triangle / Square** | **Gear Change** | Cycle through Gear 1 (35%), 2 (70%), or 3 (100%) |
| **Cross (X)** | **E-Stop** | Kills all motors and vibration immediately |
| **D-Pad (Arrows)** | **Digital Drive** | Fixed-speed movement (Left stick neutral) |

---

## 🔌 Hardware Guide (ESP32 Classic)

### Pin Mapping
| Component | Function | GPIO | Notes |
| :--- | :--- | :--- | :--- |
| **Motors (Left)** | ENA (PWM) | **25** | PWM Speed |
| | IN1 / IN2 | **26 / 27** | Direction Control |
| **Motors (Right)** | ENB (PWM) | **14** | PWM Speed |
| | IN3 / IN4 | **32 / 33** | Direction Control |
| **MPU6050** | SDA / SCL | **21 / 22** | Hardware I2C (100kHz) |
| **GPS** | RX / TX | **16 / 17** | Hardware UART1 |
| **Battery** | ADC | **34** | **ADC1 Only** (WiFi safe) |

> [!IMPORTANT]
> **I2C Diagnostics**: The firmware automatically scans for MPU6050 clones at addresses `0x68`, `0x69`, `0x20`, and `0x08`. Check the Serial Monitor on boot for the `[IMU]` logs if your sensor isn't working.

---

## 🛠️ Deployment

1.  **Clone the Repo**:
    ```bash
    git clone https://github.com/Deethunder/rc_car.git
    ```
2.  **Flash Firmware**:
    ```bash
    pio run -t upload -t monitor
    ```

---

## 📈 Dashboard Features
- **Live Speedometer**: Real-time speed from GPS.
- **Horizon Line**: Attitude Indicator using MPU6050 Fusion.
- **Battery Health**: Dynamic voltage bar with low-power alerts.
- **Motor Loads**: Visual representation of PWM duty cycles.

---

*Developed by DeeThunder Nexus Ventures.*
