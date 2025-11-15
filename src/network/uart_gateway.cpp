#include "uart_gateway.h"
#include "../logging/grind_json.h"
#include <Arduino.h>

#define STATUS_REQUEST_INTERVAL_MS 10000  // Request status every 10 seconds
#define MAX_RX_BUFFER_SIZE 512           // Maximum receive buffer size (prevent memory exhaustion)
#define MAX_JSON_SIZE 768                // Maximum JSON document size (matches session buffer + overhead)

UARTGateway::UARTGateway()
    : uart(nullptr)
    , initialized(false)
    , wifi_connected(false)
    , mqtt_connected(false)
    , last_status_request(0) {
}

void UARTGateway::init(HardwareSerial* serial, int rx_pin, int tx_pin, unsigned long baud) {
    if (!serial) {
        Serial.println("[UART Gateway] ERROR: Null serial pointer provided");
        return;
    }

    uart = serial;

    // Reserve buffer capacity to prevent heap fragmentation
    rx_buffer.reserve(MAX_RX_BUFFER_SIZE);

    // Initialize UART with error handling
    uart->begin(baud, SERIAL_8N1, rx_pin, tx_pin);

    // Verify UART is actually available
    if (!uart) {
        Serial.println("[UART Gateway] ERROR: UART initialization failed");
        initialized = false;
        return;
    }

    // Flush any garbage data
    delay(100);
    while (uart->available()) {
        uart->read();
    }

    Serial.printf("[UART Gateway] Initialized: RX=%d, TX=%d, Baud=%lu\n", rx_pin, tx_pin, baud);

    // Log free heap after initialization
    Serial.printf("[UART Gateway] Free heap: %u bytes\n", ESP.getFreeHeap());

    // Test UART write capability with a simple newline
    Serial.println("[UART Gateway] Testing UART write capability...");
    delay(50);  // Give UART time to stabilize

    uart->println("");  // Send empty line as test
    delay(50);

    Serial.println("[UART Gateway] UART write test completed");
    initialized = true;

    // Request initial status (delayed to allow gateway to boot)
    last_status_request = millis() - STATUS_REQUEST_INTERVAL_MS + 2000; // Request in 2 seconds
}

bool UARTGateway::publish_session(const GrindSession* session) {
    if (!initialized || !uart || !session) {
        Serial.println("[UART Gateway] ERROR: Not initialized or null parameters");
        return false;
    }

    // Check heap before allocating JSON
    size_t free_heap = ESP.getFreeHeap();
    if (free_heap < 8192) {  // Need at least 8KB free
        Serial.printf("[UART Gateway] ERROR: Low memory (%u bytes), skipping publish\n", free_heap);
        return false;
    }

    // Serialize session to JSON
    String json_payload;
    if (!GrindSessionSerializer::serialize_session_to_json(session, json_payload)) {
        Serial.println("[UART Gateway] Failed to serialize session");
        return false;
    }

    // Build command with fixed-size JSON document
    StaticJsonDocument<MAX_JSON_SIZE> doc;
    doc["cmd"] = "pub";

    // Parse the JSON payload into data field
    StaticJsonDocument<JSON_SESSION_BUFFER_SIZE> dataDoc;
    DeserializationError error = deserializeJson(dataDoc, json_payload);
    if (error) {
        Serial.printf("[UART Gateway] JSON parse error: %s\n", error.c_str());
        return false;
    }

    doc["data"] = dataDoc;

    bool success = send_command(doc);

    if (success) {
        Serial.printf("[UART Gateway] Sent session %lu for publishing (heap: %u bytes)\n",
                     session->session_id, ESP.getFreeHeap());
    } else {
        Serial.printf("[UART Gateway] Failed to send session %lu\n", session->session_id);
    }

    return success;
}

void UARTGateway::request_status() {
    if (!initialized || !uart) {
        Serial.println("[UART Gateway] ERROR: Cannot request status - not initialized");
        return;
    }

    StaticJsonDocument<64> doc;
    doc["cmd"] = "status";

    if (send_command(doc)) {
        last_status_request = millis();
        Serial.println("[UART Gateway] Status request sent");
    } else {
        Serial.println("[UART Gateway] ERROR: Failed to send status request");
    }
}

void UARTGateway::handle() {
    if (!initialized || !uart) {
        return;
    }

    // Safety check: verify UART is still valid
    if (!uart) {
        Serial.println("[UART Gateway] ERROR: UART pointer became null");
        initialized = false;
        return;
    }

    // Read incoming data with bounds checking
    int bytes_read = 0;
    while (uart->available() && bytes_read < 256) {  // Limit reads per cycle
        char c = uart->read();
        bytes_read++;

        if (c == '\n') {
            // Complete line received
            if (rx_buffer.length() > 0) {
                parse_response(rx_buffer);
                rx_buffer = "";
            }
        } else if (c != '\r') {
            // Prevent buffer overflow
            if (rx_buffer.length() < MAX_RX_BUFFER_SIZE - 1) {
                rx_buffer += c;
            } else {
                Serial.printf("[UART Gateway] ERROR: RX buffer overflow (%d bytes), discarding\n",
                             rx_buffer.length());
                rx_buffer = "";  // Discard corrupted data
            }
        }
    }

    // Periodically request status (TEMPORARILY DISABLED for debugging)
    // TODO: Re-enable once UART write is confirmed working
    /*
    unsigned long now = millis();
    if (now - last_status_request > STATUS_REQUEST_INTERVAL_MS) {
        request_status();
    }
    */
}

void UARTGateway::parse_response(const String& json) {
    Serial.println("[UART Gateway] Response: " + json);

    // Use fixed-size JSON document for status responses
    StaticJsonDocument<256> doc;
    DeserializationError error = deserializeJson(doc, json);

    if (error) {
        Serial.printf("[UART Gateway] JSON parse error: %s\n", error.c_str());
        return;
    }

    // Parse status update
    if (doc.containsKey("status")) {
        bool prev_wifi = wifi_connected;
        bool prev_mqtt = mqtt_connected;

        wifi_connected = doc["wifi"] | false;
        mqtt_connected = doc["mqtt"] | false;

        if (doc.containsKey("ip")) {
            ip_address = doc["ip"].as<String>();
        }

        // Log status changes
        if (wifi_connected != prev_wifi) {
            if (wifi_connected) {
                Serial.printf("[UART Gateway] WiFi connected: %s\n", ip_address.c_str());
            } else {
                Serial.println("[UART Gateway] WiFi disconnected");
            }
        }

        if (mqtt_connected != prev_mqtt) {
            if (mqtt_connected) {
                Serial.println("[UART Gateway] MQTT connected");
            } else {
                Serial.println("[UART Gateway] MQTT disconnected");
            }
        }
    }
}

bool UARTGateway::send_command(const JsonDocument& doc) {
    if (!initialized || !uart) {
        Serial.println("[UART Gateway] ERROR: Cannot send command - not initialized");
        return false;
    }

    String json;
    size_t size = serializeJson(doc, json);

    if (size == 0) {
        Serial.println("[UART Gateway] ERROR: JSON serialization failed");
        return false;
    }

    Serial.printf("[UART Gateway] Attempting to send %d bytes: %s\n", json.length(), json.c_str());

    // Send data (no pre-check, let it fail naturally if UART is broken)
    size_t written = uart->println(json);

    if (written == 0) {
        Serial.println("[UART Gateway] ERROR: UART write returned 0");
        return false;
    }

    Serial.printf("[UART Gateway] Successfully wrote %d bytes to UART\n", written);
    return true;
}
