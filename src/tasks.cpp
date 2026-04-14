#include <Arduino.h>
#include "tasks.h"
#include "config.h"
#include "telemetry.h"
#include "motor_controller.h"
#include "ps4_handler.h"
#include "imu_handler.h"
#include "gps_handler.h"
#include "battery_monitor.h"
#include "web_server.h"
#include "haptic_controller.h"   // Uses MPU6050 data already in TelemetryStore

// ─────────────────────────────────────────────────────────────
// External hardware objects (defined in main.cpp)
// ─────────────────────────────────────────────────────────────
extern MotorController  motors;
extern PS4Handler       ps4;
extern IMUHandler       imu;
extern GPSHandler       gps;
extern BatteryMonitor   battery;
extern WebServerManager webServer;

// ─────────────────────────────────────────────────────────────
// TASK 1: Control — PS4 → Motor output + Haptic feedback
// Priority: HIGHEST   Core: 1   Period: 10ms (100Hz)
//
// Haptic data flow (no extra sensor reads):
//   sensorTask (50Hz) → MPU6050 → TelemetryStore
//   controlTask (100Hz) → TelemetryStore.read() → haptic.update()
//   haptic.update() → PS4.setRumble() → controller vibrates
// ─────────────────────────────────────────────────────────────
void controlTask(void* pvParams) {
    TickType_t xLastWake = xTaskGetTickCount();

    uint8_t driveMode   = 0;    // 0 = tank, 1 = arcade
    bool    prevOptions = false;

    HapticController haptic;   // owns rumble state machine

    for (;;) {
        PS4Input in = ps4.read();
        Telemetry.setPS4Connected(in.connected);

        if (in.connected) {
            // Options button toggles drive mode
            if (in.options && !prevOptions) {
                driveMode = (driveMode == 0) ? 1 : 0;
                Serial.printf("[Control] Drive mode: %s\n",
                              driveMode == 0 ? "Tank" : "Arcade");
            }
            prevOptions = in.options;

            // Cross = emergency stop — also silences rumble immediately
            if (in.cross) {
                motors.stop();
                haptic.stopAll();
                Telemetry.setMotors(0, 0);
            } else {
                if (driveMode == 0) {
                    motors.tankDrive(in.ly, in.ry);
                } else {
                    motors.arcadeDrive(in.ly, in.lx);
                }
                Telemetry.setMotors(motors.leftSpeed(), motors.rightSpeed());
            }

            // Drive mode update
            TelemetryData snap = Telemetry.read();
            snap.drive_mode = driveMode;
            Telemetry.write(snap);

            // ── Haptic feedback ───────────────────────────────
            // Read the snapshot sensorTask already wrote — reuses
            // MPU6050 data, no duplicate I²C transaction needed.
            haptic.update(snap.ax, snap.ay, snap.az, snap.speed_kmh);

        } else {
            // Controller disconnected — safe stop + kill rumble
            motors.stop();
            haptic.stopAll();
            Telemetry.setMotors(0, 0);
        }

        vTaskDelayUntil(&xLastWake, pdMS_TO_TICKS(TaskConfig::CONTROL_PERIOD));
    }
}

// ─────────────────────────────────────────────────────────────
// TASK 2: Sensor — IMU + Battery
// Priority: HIGH   Core: 1   Period: 20ms (50Hz)
// ─────────────────────────────────────────────────────────────
void sensorTask(void* pvParams) {
    TickType_t xLastWake = xTaskGetTickCount();

    for (;;) {
        // IMU read
        IMUReading r = imu.read();
        if (r.valid) {
            Telemetry.setIMU(r.ax, r.ay, r.az,
                             r.gx, r.gy, r.gz,
                             r.roll, r.pitch);
        }

        // Battery (slower — every 10 sensor ticks = ~200ms)
        static uint8_t batTick = 0;
        if (++batTick >= 10) {
            batTick = 0;
            float v   = battery.readVoltage();
            uint8_t p = battery.readPercent();
            Telemetry.setBattery(v, p);
        }

        // System health
        TelemetryData snap = Telemetry.read();
        snap.uptime_s  = millis() / 1000;
        snap.heap_free = esp_get_free_heap_size() / 1024.0f;
        Telemetry.write(snap);

        vTaskDelayUntil(&xLastWake, pdMS_TO_TICKS(TaskConfig::SENSOR_PERIOD));
    }
}

// ─────────────────────────────────────────────────────────────
// TASK 3: GPS — Drain UART, parse NMEA
// Priority: MEDIUM   Core: 1   Period: 100ms (10Hz)
// ─────────────────────────────────────────────────────────────
void gpsTask(void* pvParams) {
    TickType_t xLastWake = xTaskGetTickCount();

    for (;;) {
        gps.update();   // drain UART buffer into TinyGPS++
        GPSReading r = gps.read();
        Telemetry.setGPS(r.latitude, r.longitude,
                          r.speed_kmh, r.altitude_m,
                          r.satellites, r.valid);

        vTaskDelayUntil(&xLastWake, pdMS_TO_TICKS(TaskConfig::GPS_PERIOD));
    }
}

// ─────────────────────────────────────────────────────────────
// TASK 4: Telemetry — Broadcast JSON over WebSocket
// Priority: LOW   Core: 0   Period: 50ms (20Hz)
//
// update() polls the raw TCP server for new connections, serves
// HTTP requests (dashboard HTML), and detects stale WS clients.
// broadcastTelemetry() then pushes a JSON frame if connected.
// ─────────────────────────────────────────────────────────────
void telemetryTask(void* pvParams) {
    TickType_t xLastWake = xTaskGetTickCount();

    for (;;) {
        // Skip telemetry while pairing to give Bluetooth 100% antenna access
        if (ps4.isPairingMode()) {
            vTaskDelay(pdMS_TO_TICKS(500));
            continue;
        }

        webServer.update();              // poll TCP, accept/cleanup
        webServer.broadcastTelemetry();  // push JSON if WS connected
        vTaskDelayUntil(&xLastWake, pdMS_TO_TICKS(TaskConfig::TELEMETRY_PERIOD));
    }
}
