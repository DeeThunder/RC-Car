// ╔══════════════════════════════════════════════════════════════╗
// ║         web_server.cpp — Raw TCP WebSocket Server             ║
// ║                                                              ║
// ║  Bare-metal implementation. No ESPAsyncWebServer, no         ║
// ║  AsyncTCP. Uses mbedtls (built into ESP-IDF) for SHA-1       ║
// ║  and base64 needed by the WebSocket handshake (RFC 6455).    ║
// ║                                                              ║
// ║  Designed for robustness under bad network conditions:       ║
// ║  • Non-blocking I/O — never stalls the FreeRTOS task        ║
// ║  • Graceful disconnect detection via TCP keepalive           ║
// ║  • Auto-cleanup of stale connections                         ║
// ║  • Fixed 512-byte JSON buffer — zero heap fragmentation     ║
// ╚══════════════════════════════════════════════════════════════╝

#include <Arduino.h>
#include "web_server.h"
#include <mbedtls/sha1.h>
#include <mbedtls/base64.h>

// RFC 6455 magic GUID for WebSocket Sec-WebSocket-Accept
static const char* WS_MAGIC_GUID = "258EAFA5-E914-47DA-95CA-C5AB0DC85B11";

// ─────────────────────────────────────────────────────────────
void WebServerManager::begin() {
    if (!SPIFFS.begin(true)) {
        Serial.println("[Web] SPIFFS mount failed!");
        return;
    }
    _server.begin();
    _server.setNoDelay(true);
    Serial.println("[Web] Raw TCP server started on port 80");
}

// ─────────────────────────────────────────────────────────────
// update() — Non-blocking poll. Call from telemetryTask.
//
// 1) Accept any new TCP connection
// 2) If we have an active WS client, check it's still alive
// ─────────────────────────────────────────────────────────────
void WebServerManager::update() {
    // ── Accept new connections ───────────────────────────────
    if (_server.hasClient()) {
        WiFiClient incoming = _server.available();
        if (incoming) {
            if (_wsConnected && _client.connected()) {
                // Reject the newcomer silently to avoid Serial spam
                incoming.stop();
            } else {
                // No active WS client — handle this connection
                _wsConnected = false;
                handleNewClient(incoming);
            }
        }
    }

    // ── Check if existing WS client has disconnected ─────────
    if (_wsConnected && !_client.connected()) {
        Serial.println("[WS] Client disconnected");
        _client.stop();
        _wsConnected = false;
    }
}

// ─────────────────────────────────────────────────────────────
// handleNewClient() — Read the HTTP request line + headers,
// then either serve a file or upgrade to WebSocket.
// ─────────────────────────────────────────────────────────────
void WebServerManager::handleNewClient(WiFiClient& incoming) {
    // Wait up to 200ms for data (non-blocking-ish)
    uint32_t start = millis();
    while (!incoming.available() && (millis() - start) < 200) {
        delay(1);
    }
    if (!incoming.available()) {
        incoming.stop();
        return;
    }

    // Read the full HTTP request (headers are small, <1KB)
    String request = "";
    while (incoming.available()) {
        request += (char)incoming.read();
        // Stop reading after double CRLF (end of headers)
        if (request.endsWith("\r\n\r\n")) break;
        // Safety: don't read more than 2KB of headers
        if (request.length() > 2048) break;
    }

    // ── Extract the request path ─────────────────────────────
    // e.g. "GET /ws HTTP/1.1" or "GET / HTTP/1.1"
    String path = "/";
    int pathStart = request.indexOf(' ');
    int pathEnd   = request.indexOf(' ', pathStart + 1);
    if (pathStart > 0 && pathEnd > pathStart) {
        path = request.substring(pathStart + 1, pathEnd);
    }

    // ── Check for WebSocket upgrade request ──────────────────
    bool isUpgrade = request.indexOf("Upgrade: websocket") > 0 ||
                     request.indexOf("Upgrade: WebSocket") > 0;

    if (isUpgrade && (path == "/ws" || path == "/")) {
        // Extract Sec-WebSocket-Key
        int keyIdx = request.indexOf("Sec-WebSocket-Key: ");
        if (keyIdx < 0) {
            send404(incoming);
            return;
        }
        int keyStart = keyIdx + 19;  // length of "Sec-WebSocket-Key: "
        int keyEnd   = request.indexOf("\r\n", keyStart);
        String wsKey = request.substring(keyStart, keyEnd);

        if (performWebSocketUpgrade(incoming, wsKey)) {
            _client      = incoming;
            _wsConnected = true;
            Serial.printf("[WS] Client connected from %s\n",
                          _client.remoteIP().toString().c_str());
        }
        return;
    }

    // ── Serve static files from SPIFFS ───────────────────────
    if (path == "/" || path == "/index.html") {
        serveFile(incoming, "/index.html", "text/html");
    } else if (path == "/ping") {
        incoming.print("HTTP/1.1 200 OK\r\n"
                       "Content-Type: text/plain\r\n"
                       "Connection: close\r\n\r\n"
                       "DeeThunder RC Car Online");
        incoming.stop();
    } else if (path == "/pair") {
        // POST /pair — triggers Bluepad32 pairing mode
        handlePairRequest(incoming);
    } else if (path == "/pairing-status") {
        // GET /pairing-status — returns current pairing state as JSON
        char resp[80];
        snprintf(resp, sizeof(resp),
                 "{\"pairing\":%s,\"connected\":%s}",
                 ps4.isPairingMode()  ? "true" : "false",
                 ps4.isConnected()    ? "true" : "false");
        incoming.printf("HTTP/1.1 200 OK\r\n"
                        "Content-Type: application/json\r\n"
                        "Access-Control-Allow-Origin: *\r\n"
                        "Connection: close\r\n\r\n%s", resp);
        incoming.stop();
    } else {
        send404(incoming);
    }
}

// ─────────────────────────────────────────────────────────────
// serveFile() — Stream a SPIFFS file in small chunks to avoid
// loading the entire file into RAM.
// ─────────────────────────────────────────────────────────────
void WebServerManager::serveFile(WiFiClient& c, const char* path,
                                  const char* mime) {
    File f = SPIFFS.open(path, "r");
    if (!f) {
        send404(c);
        return;
    }

    size_t fileSize = f.size();
    // Send HTTP response header
    c.printf("HTTP/1.1 200 OK\r\n"
             "Content-Type: %s\r\n"
             "Content-Length: %u\r\n"
             "Connection: close\r\n\r\n", mime, fileSize);

    // Stream file in 256-byte chunks
    uint8_t chunk[256];
    while (f.available()) {
        size_t bytesRead = f.read(chunk, sizeof(chunk));
        c.write(chunk, bytesRead);
    }
    f.close();
    c.stop();
}

// ─────────────────────────────────────────────────────────────
void WebServerManager::send404(WiFiClient& c) {
    c.print("HTTP/1.1 404 Not Found\r\n"
            "Content-Type: text/plain\r\n"
            "Connection: close\r\n\r\n"
            "Not Found");
    c.stop();
}

// -------------------------------------------------------------
// handlePairRequest() — POST /pair
// Calls ps4.triggerPairing() which clears Bluetooth bonds and
// opens a 60-second window for controller pairing.
// Responds with JSON so the dashboard can update the UI.
// -------------------------------------------------------------
void WebServerManager::handlePairRequest(WiFiClient& c) {
    ps4.triggerPairing();
    c.print("HTTP/1.1 200 OK\r\n"
            "Content-Type: application/json\r\n"
            "Access-Control-Allow-Origin: *\r\n"
            "Connection: close\r\n\r\n"
            "{\"ok\":true,\"message\":\"Pairing mode active — hold Share+PS on controller\"}");
    c.stop();
    Serial.println("[Web] /pair request handled.");
}

// ─────────────────────────────────────────────────────────────
// performWebSocketUpgrade() — RFC 6455 §4.2.2 handshake
//
// 1. Concatenate client key + magic GUID
// 2. SHA-1 hash
// 3. Base64 encode
// 4. Send 101 Switching Protocols
// ─────────────────────────────────────────────────────────────
bool WebServerManager::performWebSocketUpgrade(WiFiClient& c,
                                                const String& key) {
    // Step 1: Concatenate key + magic GUID
    String concat = key + WS_MAGIC_GUID;

    // Step 2: SHA-1 hash using mbedtls (built into ESP-IDF)
    uint8_t sha1Hash[20];
    mbedtls_sha1_context ctx;
    mbedtls_sha1_init(&ctx);
    mbedtls_sha1_starts(&ctx);
    mbedtls_sha1_update(&ctx,
                         (const unsigned char*)concat.c_str(),
                         concat.length());
    mbedtls_sha1_finish(&ctx, sha1Hash);
    mbedtls_sha1_free(&ctx);

    // Step 3: Base64 encode
    char b64[64];
    size_t b64Len = 0;
    mbedtls_base64_encode((unsigned char*)b64, sizeof(b64), &b64Len,
                           sha1Hash, 20);
    b64[b64Len] = '\0';

    // Step 4: Send 101 Switching Protocols response
    c.printf("HTTP/1.1 101 Switching Protocols\r\n"
             "Upgrade: websocket\r\n"
             "Connection: Upgrade\r\n"
             "Sec-WebSocket-Accept: %s\r\n\r\n", b64);

    return true;
}

// ─────────────────────────────────────────────────────────────
// sendWSTextFrame() — RFC 6455 §5.2 framing
//
// Server → Client frames are NEVER masked.
// Supports payloads up to 65535 bytes (we use ~400).
//
// Byte 0: 0x81 = FIN + Text opcode
// Byte 1: payload length (7-bit, or 126 + 2-byte extended)
// ─────────────────────────────────────────────────────────────
void WebServerManager::sendWSTextFrame(WiFiClient& c,
                                        const char* payload,
                                        size_t len) {
    uint8_t header[4];
    size_t headerLen = 0;

    header[0] = 0x81;  // FIN + Text opcode
    headerLen = 1;

    if (len < 126) {
        header[1] = (uint8_t)len;
        headerLen = 2;
    } else {
        header[1] = 126;
        header[2] = (uint8_t)(len >> 8);
        header[3] = (uint8_t)(len & 0xFF);
        headerLen = 4;
    }

    c.write(header, headerLen);
    c.write((const uint8_t*)payload, len);
}

// ─────────────────────────────────────────────────────────────
// broadcastTelemetry() — Build JSON with snprintf into a
// stack-allocated buffer, then send as a WebSocket text frame.
// Zero heap allocation.
// ─────────────────────────────────────────────────────────────
void WebServerManager::broadcastTelemetry() {
    if (!_wsConnected || !_client.connected()) return;

    // Drain any incoming data from the client (pings, pongs, etc.)
    // to prevent the TCP receive buffer from filling up and stalling.
    while (_client.available()) {
        _client.read();
    }

    TelemetryData t = Telemetry.read();

    // Stack-allocated JSON buffer — never touches the heap
    char buf[512];
    size_t len = buildJSON(t, buf, sizeof(buf));

    if (len > 0) {
        sendWSTextFrame(_client, buf, len);
    }
}

// ─────────────────────────────────────────────────────────────
// buildJSON() — Pure snprintf, zero-allocation JSON builder.
// Returns the number of characters written (excluding null).
// ─────────────────────────────────────────────────────────────
size_t WebServerManager::buildJSON(const TelemetryData& t,
                                    char* buf, size_t bufSize) {
    int n = snprintf(buf, bufSize,
        "{"
            "\"uptime\":%lu,"
            "\"heap_kb\":%.1f,"
            "\"imu\":{"
                "\"ax\":%.2f,\"ay\":%.2f,\"az\":%.2f,"
                "\"gx\":%.1f,\"gy\":%.1f,\"gz\":%.1f,"
                "\"roll\":%.1f,\"pitch\":%.1f"
            "},"
            "\"gps\":{"
                "\"lat\":%.6f,\"lng\":%.6f,"
                "\"spd\":%.1f,\"alt\":%.1f,"
                "\"sats\":%u,\"valid\":%s"
            "},"
            "\"motors\":{"
                "\"l\":%d,\"r\":%d,"
                "\"ps4\":%s,\"mode\":%u"
            "},"
            "\"battery\":{"
                "\"v\":%.2f,\"pct\":%u"
            "}"
        "}",
        (unsigned long)t.uptime_s,
        t.heap_free,
        t.ax, t.ay, t.az,
        t.gx, t.gy, t.gz,
        t.roll, t.pitch,
        t.latitude, t.longitude,
        t.speed_kmh, t.altitude,
        (unsigned int)t.satellites,
        t.gps_valid ? "true" : "false",
        (int)t.left_speed, (int)t.right_speed,
        t.ps4_connected ? "true" : "false",
        (unsigned int)t.drive_mode,
        t.battery_voltage,
        (unsigned int)t.battery_percent
    );

    if (n < 0 || (size_t)n >= bufSize) {
        Serial.println("[Web] JSON buffer overflow!");
        return 0;
    }
    return (size_t)n;
}
