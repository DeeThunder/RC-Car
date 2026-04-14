#include <Arduino.h>
#include <unity.h>
#include "gps_handler.h"
#include "web_server.h"
#include "motor_controller.h"
#include "telemetry.h"
#include "imu_handler.h"
#include "battery_monitor.h"

// ─────────────────────────────────────────────────────────────
// Test 1: GPS NMEA Parser logic
// ─────────────────────────────────────────────────────────────
void test_gps_parser_valid_gga(void) {
    GPSHandler gps;
    // Mock GPGGA sentence: Fix=1 (valid), Satellites=08, Altitude=545.4
    char mockGga[] = "GPGGA,123519,4807.038,N,01131.000,E,1,08,0.9,545.4,M,46.9,M,,*47";
    
    gps.parseSentence(mockGga);
    GPSReading r = gps.read();
    
    TEST_ASSERT_TRUE(r.has_fix);
    TEST_ASSERT_EQUAL_UINT8(8, r.satellites);
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 545.4f, r.altitude_m);
    TEST_ASSERT_DOUBLE_WITHIN(0.001, 48.1173, r.latitude); // 4807.038' -> 48.1173°
}

void test_gps_parser_no_fix(void) {
    GPSHandler gps;
    char mockGga[] = "GPGGA,123519,4807.038,N,01131.000,E,0,00,0.9,545.4,M,46.9,M,,*47";
    
    gps.parseSentence(mockGga);
    GPSReading r = gps.read();
    
    TEST_ASSERT_FALSE(r.has_fix);
    TEST_ASSERT_EQUAL_UINT8(0, r.satellites);
}

// ─────────────────────────────────────────────────────────────
// Test 2: Motor axis to speed conversion
// ─────────────────────────────────────────────────────────────
void test_motor_axis_mapping(void) {
    MotorController motors;
    
    // Test center (dead zone handling)
    TEST_ASSERT_EQUAL_INT16(0, motors.axisToSpeed(0));
    TEST_ASSERT_EQUAL_INT16(0, motors.axisToSpeed(10));
    TEST_ASSERT_EQUAL_INT16(0, motors.axisToSpeed(-10));
    
    // Test full range
    TEST_ASSERT_EQUAL_INT16(255, motors.axisToSpeed(127));
    TEST_ASSERT_EQUAL_INT16(-255, motors.axisToSpeed(-128));
}

// ─────────────────────────────────────────────────────────────
// Test 3: JSON Telemetry Builder (Zero heap check)
// ─────────────────────────────────────────────────────────────
void test_json_builder_syntax(void) {
    WebServerManager web;
    TelemetryData t;
    t.uptime_s = 1234;
    t.left_speed = 100;
    t.right_speed = -100;
    t.latitude = 45.123456;
    
    char buf[512];
    size_t len = web.buildJSON(t, buf, sizeof(buf));
    
    TEST_ASSERT_GREATER_THAN(0, len);
    TEST_ASSERT_LESS_THAN(sizeof(buf), len);
    
    // Basic syntax checks
    TEST_ASSERT_EQUAL_CHAR('{', buf[0]);
    TEST_ASSERT_EQUAL_CHAR('}', buf[len-1]);
    TEST_ASSERT_NOT_NULL(strstr(buf, "\"uptime\":1234"));
    TEST_ASSERT_NOT_NULL(strstr(buf, "\"l\":100"));
    TEST_ASSERT_NOT_NULL(strstr(buf, "\"r\":-100"));
}

// ─────────────────────────────────────────────────────────────
// Test 4: IMU Orientation Math
// ─────────────────────────────────────────────────────────────
void test_imu_orientation_math(void) {
    // rawAx, rawAy, rawAz, rawGx, rawGy, rawGz, offsets...
    // At ±2g range, 1g = 16384. 
    // Case 1: Flat (Az = 1g)
    IMUReading r = IMUHandler::computeOrientation(0, 0, 16384, 0, 0, 0, 
                                                   0, 0, 0, 0, 0, 0);
    TEST_ASSERT_FLOAT_WITHIN(1.0f, 0.0f, r.roll);
    TEST_ASSERT_FLOAT_WITHIN(1.0f, 0.0f, r.pitch);
    
    // Case 2: Tilted 90 deg roll (Ay = 1g)
    r = IMUHandler::computeOrientation(0, 16384, 0, 0, 0, 0, 
                                        0, 0, 0, 0, 0, 0);
    TEST_ASSERT_FLOAT_WITHIN(1.0f, 90.0f, r.roll);
}

// ─────────────────────────────────────────────────────────────
// Test 5: Battery Percentage Logic (3S: 9.0V - 12.6V)
// ─────────────────────────────────────────────────────────────
void test_battery_percentage_logic(void) {
    TEST_ASSERT_EQUAL_UINT8(100, BatteryMonitor::voltageToPercent(12.6f));
    TEST_ASSERT_EQUAL_UINT8(0, BatteryMonitor::voltageToPercent(9.0f));
    TEST_ASSERT_EQUAL_UINT8(50, BatteryMonitor::voltageToPercent(10.8f));
    TEST_ASSERT_EQUAL_UINT8(100, BatteryMonitor::voltageToPercent(13.0f)); // Over-voltage protection
    TEST_ASSERT_EQUAL_UINT8(0, BatteryMonitor::voltageToPercent(8.0f));    // Under-voltage protection
}

// ─────────────────────────────────────────────────────────────
// Test 6: Arcade Drive Mixing
// ─────────────────────────────────────────────────────────────
void test_arcade_drive_mixing(void) {
    MotorController motors;
    
    // Pure Forward (Throttle 127, Steer 0) → Both motors 255
    motors.arcadeDrive(127, 0);
    TEST_ASSERT_EQUAL_INT16(255, motors.leftSpeed());
    TEST_ASSERT_EQUAL_INT16(255, motors.rightSpeed());
    
    // Pure Right Spin (Throttle 0, Steer 127) → Left 255, Right -255
    motors.arcadeDrive(0, 127);
    TEST_ASSERT_EQUAL_INT16(255, motors.leftSpeed());
    TEST_ASSERT_EQUAL_INT16(-255, motors.rightSpeed());
    
    // Diagonal (Throttle 64, Steer 64) → One motor full, one motor stopped (approx)
    motors.arcadeDrive(64, 64);
    // map(64...) approx 127
    // 127 + 127 = 254 (Full Left)
    // 127 - 127 = 0   (Stop Right)
    TEST_ASSERT_INT16_WITHIN(5, 255, motors.leftSpeed());
    TEST_ASSERT_INT16_WITHIN(5, 0, motors.rightSpeed());
}

// ─────────────────────────────────────────────────────────────
void setup() {
    // Wait for serial monitor
    delay(2000);
    
    UNITY_BEGIN();
    
    RUN_TEST(test_gps_parser_valid_gga);
    RUN_TEST(test_gps_parser_no_fix);
    RUN_TEST(test_motor_axis_mapping);
    RUN_TEST(test_json_builder_syntax);
    RUN_TEST(test_imu_orientation_math);
    RUN_TEST(test_battery_percentage_logic);
    RUN_TEST(test_arcade_drive_mixing);
    
    UNITY_END();
}

void loop() {
    // Do nothing
}
