#pragma once

#include <Arduino.h>
#include <HardwareSerial.h>
#include <ArduinoJson.h>
#include "../logging/grind_logging.h"

/**
 * UARTGateway - Communication interface to ESP32-C6 WiFi/MQTT gateway
 *
 * Sends grind session data to external ESP32-C6 via UART for WiFi/MQTT publishing.
 * The ESP32-C6 handles all network connectivity, avoiding SPI conflicts with display.
 *
 * Hardware Connection:
 * ESP32-S3 TX (GPIO43) → ESP32-C6 RX (GPIO16)
 * ESP32-S3 RX (GPIO44) → ESP32-C6 TX (GPIO17)
 * GND ------------------- GND
 *
 * Protocol:
 * - JSON messages over UART
 * - S3 sends: {"cmd":"pub","data":{...session...}}
 * - C6 responds: {"status":"ok","wifi":true,"mqtt":true}
 */
class UARTGateway {
public:
    UARTGateway();

    /**
     * Initialize UART communication with gateway
     * @param uart UART peripheral to use (e.g., Serial2)
     * @param rx_pin RX GPIO pin
     * @param tx_pin TX GPIO pin
     * @param baud Baud rate (default 115200)
     */
    void init(HardwareSerial* uart, int rx_pin, int tx_pin, unsigned long baud = 115200);

    /**
     * Publish a grind session to MQTT via gateway
     * @param session Pointer to GrindSession to publish
     * @return true if command was sent successfully
     */
    bool publish_session(const GrindSession* session);

    /**
     * Request status from gateway
     */
    void request_status();

    /**
     * Handle incoming data from gateway (call periodically)
     */
    void handle();

    /**
     * Check if gateway is ready (WiFi and MQTT connected)
     */
    bool is_ready() const { return wifi_connected && mqtt_connected; }

    /**
     * Check if WiFi is connected
     */
    bool is_wifi_connected() const { return wifi_connected; }

    /**
     * Check if MQTT is connected
     */
    bool is_mqtt_connected() const { return mqtt_connected; }

    /**
     * Get gateway IP address (if connected)
     */
    String get_ip_address() const { return ip_address; }

private:
    HardwareSerial* uart;
    bool initialized;
    bool wifi_connected;
    bool mqtt_connected;
    String ip_address;
    unsigned long last_status_request;
    String rx_buffer;

    void parse_response(const String& json);
    bool send_command(const JsonDocument& doc);
};
