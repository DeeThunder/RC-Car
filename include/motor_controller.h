#pragma once

// ╔══════════════════════════════════════════════════════════════╗
// ║              motor_controller.h                              ║
// ║  Abstraction over L298N dual H-bridge.                       ║
// ║  Provides tank-drive and arcade-drive mixing.                ║
// ╚══════════════════════════════════════════════════════════════╝

#include <Arduino.h>
#include "config.h"

class MotorController {
public:
    // Initialise PWM channels and direction pins
    void begin();

    // Direct speed set — signed: positive=forward, negative=reverse
    // Range: -255 .. 255
    void setLeft(int16_t speed);
    void setRight(int16_t speed);
    void setMotors(int16_t leftSpeed, int16_t rightSpeed);

    // Tank drive: each stick controls one side independently
    // Inputs: raw PS4 axis values (-128..127)
    void tankDrive(int8_t leftY, int8_t rightY);

    // Arcade drive: one stick for throttle/turn
    // Inputs: raw PS4 axis values (-128..127)
    void arcadeDrive(int8_t throttleY, int8_t steerX);

    // Immediate stop
    void stop();

    // Current speed accessors (for telemetry)
    int16_t leftSpeed()  const { return _leftSpeed;  }
    int16_t rightSpeed() const { return _rightSpeed; }

    // ── Public for Unit Testing ──────────────────────────────
    int16_t axisToSpeed(int8_t axis) const;

private:
    int16_t _leftSpeed  = 0;
    int16_t _rightSpeed = 0;
    uint8_t _gear       = 2;  // Default to Gear 2

public:
    void setGear(uint8_t g) { _gear = constrain(g, 1, 3); }
    void incrementGear()    { if (_gear < 3) _gear++; }
    void decrementGear()    { if (_gear > 1) _gear--; }
    uint8_t currentGear() const { return _gear; }

    // Write PWM + direction to hardware
    void applyLeft(int16_t speed);
    void applyRight(int16_t speed);
};
