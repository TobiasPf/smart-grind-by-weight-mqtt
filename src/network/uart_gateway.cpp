#include "uart_gateway.h"
#include "../logging/grind_json.h"

#define STATUS_REQUEST_INTERVAL_MS 10000  // Request status every 10 seconds

UARTGateway::UARTGateway()
    : uart(nullptr)
    , initialized(false)
    , wifi_connected(false)
    , mqtt_connected(false)
    , last_status_request(0) {
}

void UARTGateway::init(HardwareSerial* serial, int rx_pin, int tx_pin, unsigned long baud) {
    uart = serial;
    uart->begin(baud, SERIAL_8N1, rx_pin, tx_pin);
    initialized = true;

    Serial.printf("[UART Gateway] Initialized: RX=%d, TX=%d, Baud=%lu\n", rx_pin, tx_pin, baud);

    // Request initial status
    request_status();
}

bool UARTGateway::publish_session(const GrindSession* session) {
    if (!initialized || !session) {
        return false;
    }

    // Serialize session to JSON
    String json_payload;
    if (!GrindSessionSerializer::serialize_session_to_json(session, json_payload)) {
        Serial.println("[UART Gateway] Failed to serialize session");
        return false;
    }

    // Build command
    JsonDocument doc;
    doc["cmd"] = "pub";

    // Parse the JSON payload into data field
    JsonDocument dataDoc;
    DeserializationError error = deserializeJson(dataDoc, json_payload);
    if (error) {
        Serial.printf("[UART Gateway] JSON parse error: %s\n", error.c_str());
        return false;
    }

    doc["data"] = dataDoc;

    bool success = send_command(doc);

    if (success) {
        Serial.printf("[UART Gateway] Sent session %lu for publishing\n", session->session_id);
    } else {
        Serial.printf("[UART Gateway] Failed to send session %lu\n", session->session_id);
    }

    return success;
}

void UARTGateway::request_status() {
    if (!initialized) return;

    JsonDocument doc;
    doc["cmd"] = "status";
    send_command(doc);

    last_status_request = millis();
}

void UARTGateway::handle() {
    if (!initialized) return;

    // Read incoming data
    while (uart->available()) {
        char c = uart->read();
        if (c == '\n') {
            // Complete line received
            if (rx_buffer.length() > 0) {
                parse_response(rx_buffer);
                rx_buffer = "";
            }
        } else if (c != '\r') {
            rx_buffer += c;
        }
    }

    // Periodically request status
    if (millis() - last_status_request > STATUS_REQUEST_INTERVAL_MS) {
        request_status();
    }
}

void UARTGateway::parse_response(const String& json) {
    Serial.println("[UART Gateway] Response: " + json);

    JsonDocument doc;
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
    if (!initialized) return false;

    String json;
    serializeJson(doc, json);

    uart->println(json);
    return true;
}
