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

// ─────────────────────────────────────────────────────────────
void setup() {
    Serial.begin(115200);
    delay(500);

    // (BLE memory cannot be released because Bluepad32 uses it to scan for controllers)

    // ── Power Efficiency & Radio Cleanup ─────────────────────
    setCpuFrequencyMhz(240);                // Max speed
    esp_coex_preference_set(ESP_COEX_PREFER_BT); // Prioritize Bluetooth
    
    Serial.println("\n╔══════════════════════════════╗");
    Serial.println("║   DeeThunder RC Car Booting  ║");
    Serial.println("╚══════════════════════════════╝");

    // Initialise hardware — order matters
    motors.begin();
    battery.begin();
    ps4.begin();

    bool imuOk = imu.begin();
    if (imuOk) imu.calibrate(300);

    display.begin(); // OLED Splash
    gps.begin();
    launchTasks();

    Serial.println("[Main] Boot complete ✓");
}

// loop() is intentionally empty — all work is in FreeRTOS tasks
void loop() {
    vTaskDelay(pdMS_TO_TICKS(10000));
}
