#include <Arduino.h>
#include "imu_handler.h"
#include "config.h"

// ─────────────────────────────────────────────────────────────
bool IMUHandler::begin() {
    Wire.begin(IMUPins::SDA, IMUPins::SCL);
    Wire.setClock(400000);  // 400kHz fast mode
    Wire.setTimeOut(5);     // STRICT: 5ms timeout to prevent hanging the RTOS task

    // Wake up MPU6050
    Wire.beginTransmission(MPU_ADDR);
    Wire.write(0x6B); // PWR_MGMT_1 register
    Wire.write(0);    // set to zero (wakes up the MPU-6050)
    if (Wire.endTransmission() != 0) {
        Serial.println("[IMU] ERROR: MPU6050 not found on I2C bus");
        return false;
    }

    // Set Accel range to ±2g (default is 0)
    Wire.beginTransmission(MPU_ADDR);
    Wire.write(0x1C); // ACCEL_CONFIG register
    Wire.write(0x00); // ±2g
    Wire.endTransmission();

    // Set Gyro range to ±250°/s (default is 0)
    Wire.beginTransmission(MPU_ADDR);
    Wire.write(0x1B); // GYRO_CONFIG register
    Wire.write(0x00); // ±250°/s
    Wire.endTransmission();

    Serial.println("[IMU] Bare-metal MPU6050 ready");
    return true;
}

// ─────────────────────────────────────────────────────────────
void IMUHandler::calibrate(uint16_t samples) {
    Serial.println("[IMU] Calibrating — keep robot still...");
    double sumAx = 0, sumAy = 0, sumAz = 0;
    double sumGx = 0, sumGy = 0, sumGz = 0;

    for (uint16_t i = 0; i < samples; i++) {
        IMUReading r = read();
        if (r.valid) {
            // Read raw scaled values (before offsets)
            // Note: read() returns offsetted/scaled values, so we need a "raw" read here
            // But to keep it simple, we'll just use the logic from the old library getMotion6
            Wire.beginTransmission(MPU_ADDR);
            Wire.write(0x3B); // starting register
            Wire.endTransmission(false);
            Wire.requestFrom(MPU_ADDR, (uint8_t)14, (uint8_t)true);

            int16_t rawAx = (Wire.read() << 8) | Wire.read();
            int16_t rawAy = (Wire.read() << 8) | Wire.read();
            int16_t rawAz = (Wire.read() << 8) | Wire.read();
            Wire.read(); Wire.read(); // Skip temperature
            int16_t rawGx = (Wire.read() << 8) | Wire.read();
            int16_t rawGy = (Wire.read() << 8) | Wire.read();
            int16_t rawGz = (Wire.read() << 8) | Wire.read();

            sumAx += (float)rawAx / ACCEL_SCALE;
            sumAy += (float)rawAy / ACCEL_SCALE;
            sumAz += (float)rawAz / ACCEL_SCALE;
            sumGx += (float)rawGx / GYRO_SCALE;
            sumGy += (float)rawGy / GYRO_SCALE;
            sumGz += (float)rawGz / GYRO_SCALE;
        }
        vTaskDelay(pdMS_TO_TICKS(2));
    }

    _axOff = sumAx / samples;
    _ayOff = sumAy / samples;
    _azOff = (sumAz / samples) - 1.0f;  // subtract 1g on Z
    _gxOff = sumGx / samples;
    _gyOff = sumGy / samples;
    _gzOff = sumGz / samples;

    Serial.printf("[IMU] Calibration done. Offsets ax=%.4f ay=%.4f az=%.4f\n",
                  _axOff, _ayOff, _azOff);
}

// ─────────────────────────────────────────────────────────────
IMUReading IMUHandler::read() {
    IMUReading r;

    // Fail-safe: if we are in a cooldown period, just return invalid
    // Tightened to 3 errors for faster failover during sensitive handshakes
    if (_consecutiveErrors >= 3 && (millis() - _lastFailMs < 5000)) {
        return r; 
    }

    Wire.beginTransmission(MPU_ADDR);
    Wire.write(0x3B); 
    if (Wire.endTransmission(false) != 0) {
        _consecutiveErrors++;
        _lastFailMs = millis();
        return r; 
    }
    
    Wire.requestFrom(MPU_ADDR, (uint8_t)14, (uint8_t)true);
    if (Wire.available() < 14) {
        _consecutiveErrors++;
        _lastFailMs = millis();
        return r;
    }

    // Reset on success
    _consecutiveErrors = 0;

    int16_t rax = (Wire.read() << 8) | Wire.read();
    int16_t ray = (Wire.read() << 8) | Wire.read();
    int16_t raz = (Wire.read() << 8) | Wire.read();
    Wire.read(); Wire.read(); // Skip temp
    int16_t rgx = (Wire.read() << 8) | Wire.read();
    int16_t rgy = (Wire.read() << 8) | Wire.read();
    int16_t rgz = (Wire.read() << 8) | Wire.read();

    return computeOrientation(rax, ray, raz, rgx, rgy, rgz,
                              _axOff, _ayOff, _azOff,
                              _gxOff, _gyOff, _gzOff);
}

// ─────────────────────────────────────────────────────────────
IMUReading IMUHandler::computeOrientation(int16_t rax, int16_t ray, int16_t raz,
                                          int16_t rgx, int16_t rgy, int16_t rgz,
                                          float axOff, float ayOff, float azOff,
                                          float gxOff, float gyOff, float gzOff) {
    IMUReading r;

    // Convert raw → physical units (g and °/s), apply calibration offsets
    r.ax = ((float)rax / ACCEL_SCALE - axOff) * G_TO_MS2;
    r.ay = ((float)ray / ACCEL_SCALE - ayOff) * G_TO_MS2;
    r.az = ((float)raz / ACCEL_SCALE - azOff) * G_TO_MS2;
    r.gx = (float)rgx / GYRO_SCALE - gxOff;
    r.gy = (float)rgy / GYRO_SCALE - gyOff;
    r.gz = (float)rgz / GYRO_SCALE - gzOff;

    // Tilt angles from accelerometer
    r.roll  = atan2f(r.ay, r.az) * 180.0f / M_PI;
    r.pitch = atan2f(-r.ax, sqrtf(r.ay * r.ay + r.az * r.az)) * 180.0f / M_PI;

    r.valid = true;
    return r;
}
