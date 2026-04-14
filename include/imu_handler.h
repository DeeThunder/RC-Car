#pragma once

// ╔══════════════════════════════════════════════════════════════╗
// ║              imu_handler.h                                   ║
// ║  Bare-metal MPU6050 driver using raw Wire.h (I2C) commands.  ║
// ║  Removes dependency on the bloated MPU6050 library.          ║
// ╚══════════════════════════════════════════════════════════════╝

#include <Wire.h>
#include <cmath>

struct IMUReading {
    float ax, ay, az;      // m/s²  (calibrated)
    float gx, gy, gz;      // °/s   (calibrated)
    float roll, pitch;     // degrees (from accel)
    bool  valid = false;
};

class IMUHandler {
public:
    bool begin();

    // Read and return latest data
    IMUReading read();

    // Pure logic for unit testing (accel/gyro raw scaled → roll/pitch/units)
    static IMUReading computeOrientation(int16_t rax, int16_t ray, int16_t raz,
                                       int16_t rgx, int16_t rgy, int16_t rgz,
                                       float axOff, float ayOff, float azOff,
                                       float gxOff, float gyOff, float gzOff);

    // Run a simple offset calibration (call once at startup, robot still)
    void calibrate(uint16_t samples = 500);

private:
    static constexpr uint8_t MPU_ADDR = 0x68;

    // Calibration offsets
    float _axOff = 0, _ayOff = 0, _azOff = 0;
    float _gxOff = 0, _gyOff = 0, _gzOff = 0;

    static constexpr float ACCEL_SCALE = 16384.0f;  // ±2g
    static constexpr float GYRO_SCALE  = 131.0f;    // ±250°/s
    static constexpr float G_TO_MS2    = 9.81f;
};
