#pragma once

// ╔══════════════════════════════════════════════════════════════╗
// ║              tasks.h                                         ║
// ║  FreeRTOS task declarations. Each task owns one concern:     ║
// ║    • controlTask   — PS4 → motor output (Core 1, highest)   ║
// ║    • sensorTask    — IMU read + battery sample (Core 1)      ║
// ║    • gpsTask       — GPS UART drain + parse (Core 1)         ║
// ║    • telemetryTask — WebSocket broadcast (Core 0)            ║
// ╚══════════════════════════════════════════════════════════════╝

#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

// Task entry points (registered in main.cpp)
void controlTask   (void* pvParams);
void sensorTask    (void* pvParams);
void gpsTask       (void* pvParams);
void displayTask   (void* pvParams);
