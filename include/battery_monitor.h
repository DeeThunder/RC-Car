#pragma once

// ╔══════════════════════════════════════════════════════════════╗
// ║              battery_monitor.h                               ║
// ║  Reads voltage divider on ADC, returns voltage + percentage  ║
// ╚══════════════════════════════════════════════════════════════╝

#include <Arduino.h>
#include "config.h"

class BatteryMonitor {
public:
    void begin() {
        pinMode(BatteryConfig::ADC_PIN, INPUT);
        Serial.println("[Battery] Monitor ready");
    }

    // Read oversampled voltage
    float readVoltage() {
        uint32_t sum = 0;
        for (uint8_t i = 0; i < BatteryConfig::SAMPLE_COUNT; i++) {
            sum += analogRead(BatteryConfig::ADC_PIN);
            delayMicroseconds(100);
        }
        float raw = (float)sum / BatteryConfig::SAMPLE_COUNT;
        float vADC = (raw / BatteryConfig::ADC_MAX) * BatteryConfig::ADC_VREF;
        return vADC * BatteryConfig::DIVIDER_RATIO;
    }

    static uint8_t voltageToPercent(float v) {
        float pct = (v - BatteryConfig::BATTERY_MIN_V)
                  / (BatteryConfig::BATTERY_MAX_V - BatteryConfig::BATTERY_MIN_V)
                  * 100.0f;
        return (uint8_t)constrain((int)pct, 0, 100);
    }

    uint8_t readPercent() {
        return voltageToPercent(readVoltage());
    }
};
