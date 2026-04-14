#include <Arduino.h>
#include "motor_controller.h"

// ─────────────────────────────────────────────────────────────
void MotorController::begin() {
    // Setup PWM channels
    ledcSetup(MotorPWM::CHANNEL_L, MotorPWM::FREQUENCY, MotorPWM::RESOLUTION);
    ledcSetup(MotorPWM::CHANNEL_R, MotorPWM::FREQUENCY, MotorPWM::RESOLUTION);
    ledcAttachPin(MotorPins::ENA, MotorPWM::CHANNEL_L);
    ledcAttachPin(MotorPins::ENB, MotorPWM::CHANNEL_R);

    // Direction pins
    pinMode(MotorPins::IN1, OUTPUT);
    pinMode(MotorPins::IN2, OUTPUT);
    pinMode(MotorPins::IN3, OUTPUT);
    pinMode(MotorPins::IN4, OUTPUT);

    stop();
    Serial.println("[Motor] Initialised");
}

// ─────────────────────────────────────────────────────────────
void MotorController::stop() {
    applyLeft(0);
    applyRight(0);
}

// ─────────────────────────────────────────────────────────────
void MotorController::setLeft(int16_t speed) {
    _leftSpeed = constrain(speed, -MotorPWM::MAX_SPEED, MotorPWM::MAX_SPEED);
    applyLeft(_leftSpeed);
}

void MotorController::setRight(int16_t speed) {
    _rightSpeed = constrain(speed, -MotorPWM::MAX_SPEED, MotorPWM::MAX_SPEED);
    applyRight(_rightSpeed);
}

void MotorController::setMotors(int16_t l, int16_t r) {
    setLeft(l);
    setRight(r);
}

// ─────────────────────────────────────────────────────────────
// Tank: left stick Y → left wheels,  right stick Y → right wheels
void MotorController::tankDrive(int8_t leftY, int8_t rightY) {
    setLeft(axisToSpeed(leftY));
    setRight(axisToSpeed(rightY));
}

// Arcade: single stick — Y=throttle, X=steer
void MotorController::arcadeDrive(int8_t throttleY, int8_t steerX) {
    int16_t throttle = axisToSpeed(throttleY);
    int16_t steer    = axisToSpeed(steerX);
    int16_t l = constrain(throttle + steer, -255, 255);
    int16_t r = constrain(throttle - steer, -255, 255);
    setMotors(l, r);
}

// ─────────────────────────────────────────────────────────────
// Private helpers
// ─────────────────────────────────────────────────────────────

int16_t MotorController::axisToSpeed(int8_t axis) const {
    // Apply dead zone
    if (abs(axis) < PS4Config::DEAD_ZONE) return 0;

    // Remap -128..127 → -255..255, preserving sign
    int16_t rawSpeed = (int16_t)map(axis, -128, 127, -255, 255);

    // Apply gear scaling
    float multiplier = 1.0f;
    if      (_gear == 1) multiplier = 0.35f; // Slow
    else if (_gear == 2) multiplier = 0.70f; // Normal
    else                 multiplier = 1.00f; // Turbo

    return (int16_t)(rawSpeed * multiplier);
}

void MotorController::applyLeft(int16_t speed) {
    if (speed > 0) {
        digitalWrite(MotorPins::IN1, HIGH);
        digitalWrite(MotorPins::IN2, LOW);
    } else if (speed < 0) {
        digitalWrite(MotorPins::IN1, LOW);
        digitalWrite(MotorPins::IN2, HIGH);
        speed = -speed;
    } else {
        // Brake: both low = coast, both high = brake
        digitalWrite(MotorPins::IN1, LOW);
        digitalWrite(MotorPins::IN2, LOW);
    }
    ledcWrite(MotorPWM::CHANNEL_L, (uint8_t)speed);
}

void MotorController::applyRight(int16_t speed) {
    if (speed > 0) {
        digitalWrite(MotorPins::IN3, HIGH);
        digitalWrite(MotorPins::IN4, LOW);
    } else if (speed < 0) {
        digitalWrite(MotorPins::IN3, LOW);
        digitalWrite(MotorPins::IN4, HIGH);
        speed = -speed;
    } else {
        digitalWrite(MotorPins::IN3, LOW);
        digitalWrite(MotorPins::IN4, LOW);
    }
    ledcWrite(MotorPWM::CHANNEL_R, (uint8_t)speed);
}
