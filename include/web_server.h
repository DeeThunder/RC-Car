#pragma once

// ╔══════════════════════════════════════════════════════════════╗
// ║              web_server.h — Raw TCP WebSocket Server          ║
// ║                                                              ║
// ║  Zero-dependency WebSocket implementation using WiFiServer.  ║
// ║  Serves SPIFFS dashboard via chunked HTTP, then upgrades     ║
// ║  to raw WebSocket for telemetry push. Single-client only     ║
// ║  to minimise heap usage alongside PS4 Bluetooth Classic.     ║
// ╚══════════════════════════════════════════════════════════════╝

#include <WiFi.h>
#include <WiFiClient.h>
#include <WiFiServer.h>
#include <SPIFFS.h>
#include "telemetry.h"
#include "config.h"

class WebServerManager {
public:
    void begin();

    // Must be called frequently from the telemetry task.
    // Accepts new TCP connections, handles HTTP and WS frames.
    void update();

    // Push telemetry JSON to the connected WebSocket client.
    void broadcastTelemetry();

    bool hasClient() const { return _wsConnected; }

    // ── Public for Unit Testing ──────────────────────────────
    size_t buildJSON(const TelemetryData& t, char* buf, size_t bufSize);

private:
    WiFiServer _server{WebConfig::HTTP_PORT};
    WiFiClient _client;           // single active connection
    bool       _wsConnected = false;   // true after WS upgrade

    // ── HTTP handling ────────────────────────────────────────
    void handleNewClient(WiFiClient& incoming);
    void serveFile(WiFiClient& c, const char* path, const char* mime);
    void send404(WiFiClient& c);

    // ── WebSocket handshake & framing ────────────────────────
    bool performWebSocketUpgrade(WiFiClient& c, const String& key);
    void sendWSTextFrame(WiFiClient& c, const char* payload, size_t len);

};
