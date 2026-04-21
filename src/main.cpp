// ╔══════════════════════════════════════════════════════════════╗
// ║              main.cpp — Application Entry Point              ║
// ║                                                              ║
// ║  This file is the ORCHESTRATOR only.                         ║
// ║  It creates hardware objects, initialises them in the        ║
// ║  right order, and launches FreeRTOS cockpit tasks.           ║
// ║  No business logic lives here.                               ║
// ╚══════════════════════════════════════════════════════════════╝

#include <Arduino.h>
#include "esp_bt.h"
#include "esp_coexist.h"

#include "config.h"
#include "telemetry.h"
#include "motor_controller.h"
#include "ps4_handler.h"
#include "imu_handler.h"
#include "gps_handler.h"
#include "battery_monitor.h"
#include "display_handler.h"
#include "tasks.h"

// ── Hardware singletons ───────────────────────────────────────
ControllerPtr PS4Handler::_controller = nullptr;
int           PS4Handler::_failCount  = 0;
MotorController  motors;
PS4Handler       ps4;
IMUHandler       imu;
GPSHandler       gps;
BatteryMonitor   battery;
DisplayHandler    display;

// ─────────────────────────────────────────────────────────────
// FreeRTOS Task Launcher

// ─────────────────────────────────────────────────────────────
static void launchTasks() {
    // Control task — highest priority, Core 1
    xTaskCreatePinnedToCore(
        controlTask, "Control",
        TaskConfig::CONTROL_STACK,
        nullptr,
        TaskConfig::CONTROL_PRIORITY,
        nullptr,
        TaskConfig::CORE_1
    );

    // Sensor task — Core 1
    xTaskCreatePinnedToCore(
        sensorTask, "Sensor",
        TaskConfig::SENSOR_STACK,
        nullptr,
        TaskConfig::SENSOR_PRIORITY,
        nullptr,
        TaskConfig::CORE_1
    );

    // GPS task — Core 1
    xTaskCreatePinnedToCore(
        gpsTask, "GPS",
        TaskConfig::GPS_STACK,
        nullptr,
        TaskConfig::GPS_PRIORITY,
        nullptr,
        TaskConfig::CORE_1
    );

    // Task 4: Physical Dashboard (OLED)
    xTaskCreatePinnedToCore(
        displayTask, "Display",
        4096,               // Moderate stack for Graphics
        nullptr,
        3,                  // Low priority
        nullptr,
        TaskConfig::CORE_1  // Run on same core as sensors
    );

    Serial.println("[Main] All tasks launched");
}

#include "soc/soc.h"
#include "soc/rtc_cntl_reg.h"

// ─────────────────────────────────────────────────────────────
void setup() {
    // Disable brownout detector. Opening the Serial Monitor pulls DTR/RTS which resets the ESP32.
    // Over a weak USB port, this reset + RF calibration causes a tiny voltage dip that triggers
    // the brownout detector, causing an infinite loop. Disabling it lets it ride out the dip safely.
    WRITE_PERI_REG(RTC_CNTL_BROWN_OUT_REG, 0);

    Serial.begin(115200);
    delay(500);

    // (BLE memory cannot be released because Bluepad32 uses it to scan for controllers)

    // ── Power Efficiency & Radio Cleanup ─────────────────────
    setCpuFrequencyMhz(240);                // Max speed
    esp_coex_preference_set(ESP_COEX_PREFER_BT); // Prioritize Bluetooth
    
    Serial.println("\n╔══════════════════════════════╗");
    Serial.println("║   DeeThunder RC Car Booting  ║");
    Serial.println("╚══════════════════════════════╝");

    // Initialise hardware — order matters for power spikes!
    // 1. Setup basic PWM and ADC (Low power)
    motors.begin();
    battery.begin();

    // 2. Setup display and I2C first so user sees booting status
    Serial.println("\n[Main] Scanning I2C bus for display:");
    uint8_t displayAddr = 0x3C;
    bool found3C = false, found3D = false;
    
    // Wire.begin is needed for scanner since we moved imu.begin() down
    Wire.begin(IMUPins::SDA, IMUPins::SCL); 
    
    for(byte address = 1; address < 127; address++ ) {
        Wire.beginTransmission(address);
        if (Wire.endTransmission() == 0) {
            Serial.printf("[Main] I2C device found at address 0x%02X\n", address);
            if (address == 0x3C) found3C = true;
            if (address == 0x3D) found3D = true;
        }
    }

    if (found3D && !found3C) displayAddr = 0x3D;

    display.begin(displayAddr); // Turns on OLED charge pump
    display.drawSplash(); 
    delay(1000); // 1-second delay lets the OLED charge pump power stabilize
    
    // 3. Initialize IMU
    bool imuOk = imu.begin();
    if (imuOk) imu.calibrate(300);

    // 4. Initialize BT Radio (MASSIVE 500mA spike)
    Serial.println("[Main] Starting Bluetooth Radio...");
    ps4.begin();
    
    gps.begin();
    launchTasks();

    Serial.println("[Main] Boot complete ✓");
}

// loop() is intentionally empty — all work is in FreeRTOS tasks
void loop() {
    vTaskDelay(pdMS_TO_TICKS(10000));
}
