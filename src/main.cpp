// ╔══════════════════════════════════════════════════════════════╗
// ║              main.cpp — Application Entry Point              ║
// ║                                                              ║
// ║  This file is the ORCHESTRATOR only.                         ║
// ║  It creates hardware objects, initialises them in the        ║
// ║  right order, connects to WiFi, and launches FreeRTOS        ║
// ║  tasks. No business logic lives here.                        ║
// ╚══════════════════════════════════════════════════════════════╝

#include <Arduino.h>
#include <WiFi.h>
#include "esp_bt.h"

#include "config.h"
#include "telemetry.h"
#include "motor_controller.h"
#include "ps4_handler.h"
#include "imu_handler.h"
#include "gps_handler.h"
#include "battery_monitor.h"
#include "web_server.h"
#include "tasks.h"

// ── Hardware singletons ───────────────────────────────────────
MotorController  motors;
PS4Handler       ps4;
IMUHandler       imu;
GPSHandler       gps;
BatteryMonitor   battery;
WebServerManager webServer;

// ─────────────────────────────────────────────────────────────
static void connectWiFi() {
    Serial.printf("[WiFi] Connecting to %s", WiFiConfig::SSID);
    WiFi.setHostname(WiFiConfig::HOSTNAME);
    WiFi.begin(WiFiConfig::SSID, WiFiConfig::PASSWORD);

    uint8_t attempts = 0;
    while (WiFi.status() != WL_CONNECTED && attempts < 30) {
        delay(500);
        Serial.print(".");
        attempts++;
    }

    if (WiFi.status() == WL_CONNECTED) {
        Serial.printf("\n[WiFi] Connected! IP: http://%s\n",
                      WiFi.localIP().toString().c_str());
    } else {
        Serial.println("\n[WiFi] Failed — starting AP mode");
        WiFi.softAP("DeeThunderRC", "rccar1234");
        Serial.printf("[WiFi] AP IP: http://%s\n",
                      WiFi.softAPIP().toString().c_str());
    }
}

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

    // Telemetry / WebSocket task — Core 0 (alongside WiFi stack)
    xTaskCreatePinnedToCore(
        telemetryTask, "Telemetry",
        TaskConfig::TELEMETRY_STACK,
        nullptr,
        TaskConfig::TELEMETRY_PRIORITY,
        nullptr,
        TaskConfig::CORE_0
    );

    Serial.println("[Main] All tasks launched");
}

// ─────────────────────────────────────────────────────────────
void setup() {
    Serial.begin(115200);
    delay(500);

    // Release unused BLE memory for Classic ESP32 heap optimization
    esp_bt_controller_mem_release(ESP_BT_MODE_BLE);

    // ── Power Efficiency ─────────────────────────────────────
    setCpuFrequencyMhz(160);                // Drop from 240MHz to 160MHz
    WiFi.setSleep(WIFI_PS_MIN_MODEM);       // Enable Wi-Fi modem sleep
    esp_bt_sleep_enable();                  // Enable BT power management
    
    Serial.println("\n╔══════════════════════════════╗");
    Serial.println("║   DeeThunder RC Car Booting  ║");
    Serial.println("╚══════════════════════════════╝");

    // Initialise hardware — order matters
    motors.begin();
    battery.begin();
    ps4.begin();

    bool imuOk = imu.begin();
    if (imuOk) imu.calibrate(300);

    gps.begin();
    connectWiFi();
    webServer.begin();
    launchTasks();

    Serial.println("[Main] Boot complete ✓");
}

// loop() is intentionally empty — all work is in FreeRTOS tasks
void loop() {
    vTaskDelay(pdMS_TO_TICKS(10000));
}
