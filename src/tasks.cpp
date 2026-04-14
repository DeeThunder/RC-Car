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

    uint8_t driveMode    = 0;    // 0 = tank, 1 = arcade
    bool    prevTriangle = false;
    bool    prevSquare   = false;
    bool    prevCircle   = false;
    bool    prevOptions  = false;

    HapticController haptic;   // owns rumble state machine

    for (;;) {
        PS4Input in = ps4.read();
        Telemetry.setPS4Connected(in.connected);

        if (in.connected) {
            // Options OR Circle button toggles drive mode
            if ((in.options && !prevOptions) || (in.circle && !prevCircle)) {
                driveMode = (driveMode == 0) ? 1 : 0;
                Serial.printf("[Control] Drive mode: %s\n",
                              driveMode == 0 ? "Tank" : "Arcade");
            }
            prevOptions = in.options;
            prevCircle  = in.circle;

            // Triangle / Square cycle gears
            if (in.triangle && !prevTriangle) {
                motors.incrementGear();
                Serial.printf("[Control] Gear UP: %d\n", motors.currentGear());
            }
            if (in.square && !prevSquare) {
                motors.decrementGear();
                Serial.printf("[Control] Gear DOWN: %d\n", motors.currentGear());
            }
            prevTriangle = in.triangle;
            prevSquare   = in.square;

            // Cross = emergency stop — also silences rumble immediately
            if (in.cross) {
                motors.stop();
                haptic.stopAll();
                Telemetry.setMotors(0, 0);

            } else {
                // Handle D-Pad (Arrows) movement if sticks are neutral
                bool sticksNeutral = (abs(in.ly) < PS4Config::DEAD_ZONE &&
                                      abs(in.ry) < PS4Config::DEAD_ZONE &&
                                      abs(in.lx) < PS4Config::DEAD_ZONE);

                if (sticksNeutral && in.dpad != 0) {
                    // Map D-Pad to virtual axis values
                    int8_t vLy = 0, vRy = 0, vLx = 0;
                    if (in.dpad & 0x01) { vLy = 127; vRy = 127; } // Up
                    if (in.dpad & 0x02) { vLy = -128; vRy = -128; } // Down
                    if (in.dpad & 0x04) { vLx = -128; } // Left
                    if (in.dpad & 0x08) { vLx = 127; }  // Right

                    if (driveMode == 0) motors.tankDrive(vLy, vRy);
                    else                motors.arcadeDrive(vLy, vLx);
                } else {
                    // Stick movement
                    if (driveMode == 0) motors.tankDrive(in.ly, in.ry);
                    else                motors.arcadeDrive(in.ly, in.lx);
                }
                Telemetry.setMotors(motors.leftSpeed(), motors.rightSpeed());
            }

            // Telemetry state update
            TelemetryData snap = Telemetry.read();
            snap.drive_mode = driveMode;
            snap.gear       = motors.currentGear();
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
        // TOTAL SILENCE: Kill all sensor activity during pairing window
        if (ps4.isPairingMode()) {
            vTaskDelay(pdMS_TO_TICKS(500));
            continue;
        }

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
        // TOTAL SILENCE: Kill GPS task during pairing window
        if (ps4.isPairingMode()) {
            vTaskDelay(pdMS_TO_TICKS(500));
            continue;
        }

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

        // THROTLE WiFi: If no controller is connected, stop telemetry until 
        // connection is established. This keeps antenna clear for BT handshake.
        if (!ps4.isConnected()) {
            // EXTREME SILENCE: Pause almost all WiFi to ensure handshake success.
            // Only poll server once every 2 seconds.
            static uint8_t silenceTick = 0;
            if (++silenceTick >= 8) { // 250ms * 8 = 2000ms
                silenceTick = 0;
                webServer.update(); 
            }
            vTaskDelayUntil(&xLastWake, pdMS_TO_TICKS(TaskConfig::TELEMETRY_PERIOD));
            continue;
        }

        webServer.update();              // poll TCP, accept/cleanup
        webServer.broadcastTelemetry();  // push JSON if WS connected
        vTaskDelayUntil(&xLastWake, pdMS_TO_TICKS(TaskConfig::TELEMETRY_PERIOD));
    }
}
