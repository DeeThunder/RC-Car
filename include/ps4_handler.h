#pragma once

// ╔══════════════════════════════════════════════════════════════╗
// ║              ps4_handler.h                                   ║
// ║  Wraps PS4-esp32 library. Exposes clean input struct.        ║
// ╚══════════════════════════════════════════════════════════════╝

#include <PS4Controller.h>
#include "config.h"

struct PS4Input {
    int8_t  lx = 0, ly = 0;      // Left stick  -128..127
    int8_t  rx = 0, ry = 0;      // Right stick -128..127
    uint8_t l2 = 0, r2 = 0;      // Analog triggers 0..255
    bool    cross   = false;
    bool    circle  = false;
    bool    square  = false;
    bool    triangle= false;
    bool    l1 = false, r1 = false;
    bool    options = false;
    bool    connected = false;
};

class PS4Handler {
public:
    void begin() {
        PS4.begin(PS4Config::MAC_ADDRESS);
        Serial.println("[PS4] Bluetooth ready. Pair your controller.");
    }

    PS4Input read() const {
        PS4Input in;
        in.connected = PS4.isConnected();
        if (!in.connected) return in;

        in.lx       = PS4.LStickX();
        in.ly       = PS4.LStickY();
        in.rx       = PS4.RStickX();
        in.ry       = PS4.RStickY();
        in.l2       = PS4.L2Value();
        in.r2       = PS4.R2Value();
        in.cross    = PS4.Cross();
        in.circle   = PS4.Circle();
        in.square   = PS4.Square();
        in.triangle = PS4.Triangle();
        in.l1       = PS4.L1();
        in.r1       = PS4.R1();
        in.options  = PS4.Options();
        return in;
    }

    bool isConnected() const { return PS4.isConnected(); }
};
