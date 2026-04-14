#pragma once

// ╔══════════════════════════════════════════════════════════════╗
// ║                secret_example.h — Template                   ║
// ║  Rename this file to 'secret.h' and enter your credentials.  ║
// ║  'secret.h' is ignored by git to keep your info safe.        ║
// ╚══════════════════════════════════════════════════════════════╝

namespace WiFiConfig {
    constexpr const char* SSID     = "YOUR_WIFI_SSID";
    constexpr const char* PASSWORD = "YOUR_WIFI_PASSWORD";
    constexpr const char* HOSTNAME = "deethunder-car";
}

namespace PS4Config {
    // Run the PS4 pairing sketch to get this address
    constexpr const char* MAC_ADDRESS = "XX:XX:XX:XX:XX:XX";
}
