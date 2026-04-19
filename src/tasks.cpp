#include <Arduino.h>
#include "tasks.h"
#include "config.h"
#include "telemetry.h"
#include "motor_controller.h"
#include "ps4_handler.h"
#include "imu_handler.h"
#include "gps_handler.h"
#include "battery_monitor.h"
#include "display_handler.h"
#include "haptic_controller.h"

// ─────────────────────────────────────────────────────────────
// External hardware objects (defined in main.cpp)
// ─────────────────────────────────────────────────────────────
extern MotorController  motors;
extern PS4Handler       ps4;
extern IMUHandler       imu;
extern GPSHandler       gps;
extern BatteryMonitor   battery;
extern DisplayHandler   display;

// ─────────────────────────────────────────────────────────────
// TASK 1: Control — PS4 → Motor output + Haptic feedback
// Priority: HIGHEST   Core: 1   Period: 10ms (100Hz)
// ─────────────────────────────────────────────────────────────
void controlTask(void* pvParams) {
    TickType_t xLastWake = xTaskGetTickCount();

    uint8_t driveMode    = 1;    // 0 = tank, 1 = arcade
    bool    prevTriangle = false;
    bool    prevSquare   = false;
    bool    prevCircle   = false;
    bool    prevOptions  = false;

    HapticController haptic; 

    for (;;) {
        PS4Input in = ps4.read();
        Telemetry.setPS4Connected(in.connected);

        if (in.connected) {
            // Mode toggle
            if ((in.options && !prevOptions) || (in.circle && !prevCircle)) {
                driveMode = (driveMode == 0) ? 1 : 0;
            }
            prevOptions = in.options;
            prevCircle  = in.circle;

            // Gear cycle
            if (in.triangle && !prevTriangle) motors.incrementGear();
            if (in.square && !prevSquare)     motors.decrementGear();
            prevTriangle = in.triangle;
            prevSquare   = in.square;

            if (in.cross) {
                motors.stop();
                haptic.stopAll();
                Telemetry.setMotors(0, 0);
            } else {
                if (driveMode == 0) motors.tankDrive(in.ly, in.ry);
                else                motors.arcadeDrive(in.ly, in.lx);
                Telemetry.setMotors(motors.leftSpeed(), motors.rightSpeed());
            }

            // Sync telemetry snapshot for other tasks
            TelemetryData snap = Telemetry.read();
            snap.drive_mode = driveMode;
            snap.gear       = motors.currentGear();
            Telemetry.write(snap);

            // ── Haptic Telemetry ──────────────────────────────
            // 1. On-Car Rumble simulation
            haptic.update(snap.ax, snap.ay, snap.az, snap.speed_kmh);
            // 2. Controller Light Bar & Rumble feedback (The "Sensory" Dash)
            ps4.updateHaptics(snap);

        } else {
            motors.stop();
            haptic.stopAll();
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
        IMUReading r = imu.read();
        if (r.valid) {
            Telemetry.setIMU(r.ax, r.ay, r.az, r.gx, r.gy, r.gz, r.roll, r.pitch);
        }

        static uint8_t batTick = 0;
        if (++batTick >= 10) {
            batTick = 0;
            Telemetry.setBattery(battery.readVoltage(), battery.readPercent());
        }

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
        gps.update();
        GPSReading r = gps.read();
        Telemetry.setGPS(r.latitude, r.longitude, r.speed_kmh, r.altitude_m, r.satellites, r.valid);
        vTaskDelayUntil(&xLastWake, pdMS_TO_TICKS(TaskConfig::GPS_PERIOD));
    }
}

// ─────────────────────────────────────────────────────────────
// TASK 4: Display — OLED Rendering
// Priority: LOW   Core: 1   Period: 200ms (5Hz)
// ─────────────────────────────────────────────────────────────
void displayTask(void* pvParams) {
    TickType_t xLastWake = xTaskGetTickCount();
    
    for (;;) {
        TelemetryData snap = Telemetry.read();

        if (ps4.isPairingMode()) {
            display.drawPairingMode();
        } else {
            display.drawDashboard(snap);
        }

        vTaskDelayUntil(&xLastWake, pdMS_TO_TICKS(DisplayConfig::REFRESH_PERIOD));
    }
}
