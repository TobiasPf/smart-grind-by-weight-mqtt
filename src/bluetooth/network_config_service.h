#pragma once

#include <Arduino.h>
#include <Preferences.h>
#include <ArduinoJson.h>
#include "../config/constants.h"
#include "../network/wifi_manager.h"
#include "../network/mqtt_manager.h"

// Forward declarations
class WiFiManager;
class MQTTManager;

/**
 * Network control commands (sent via control characteristic)
 */
enum class NetworkControlCommand : uint8_t {
    ENABLE_WIFI = 0x01,
    DISABLE_WIFI = 0x02,
    ENABLE_MQTT = 0x03,
    DISABLE_MQTT = 0x04,
    TEST_CONNECTION = 0x05,
    GET_STATUS = 0x06
};

/**
 * NetworkConfigService - BLE service for WiFi/MQTT provisioning
 *
 * Provides BLE characteristics for configuring WiFi credentials and MQTT
 * broker settings, as well as querying network status and controlling
 * network connectivity.
 */
class NetworkConfigService {
public:
    NetworkConfigService();
    ~NetworkConfigService();

    /**
     * Initialize with WiFi and MQTT managers
     * @param wifi WiFiManager instance
     * @param mqtt MQTTManager instance
     */
    void init(WiFiManager* wifi, MQTTManager* mqtt);

    /**
     * Handle WiFi credentials write
     * Format: "SSID|password"
     * @param data Data buffer
     * @param length Data length
     * @return true if credentials were set successfully
     */
    bool handle_wifi_credentials_write(const uint8_t* data, size_t length);

    /**
     * Handle MQTT config write
     * Format: "host:port|username|password"
     * @param data Data buffer
     * @param length Data length
     * @return true if config was set successfully
     */
    bool handle_mqtt_config_write(const uint8_t* data, size_t length);

    /**
     * Handle control command write
     * @param data Data buffer
     * @param length Data length
     * @return true if command was executed successfully
     */
    bool handle_control_write(const uint8_t* data, size_t length);

    /**
     * Get current network status as JSON string
     * @param out Output string for JSON status
     * @return true if status was generated successfully
     */
    bool get_status_json(String& out);

    /**
     * Handle status characteristic read
     * @param out Output buffer
     * @param max_len Maximum buffer length
     * @return Number of bytes written to buffer
     */
    size_t handle_status_read(uint8_t* out, size_t max_len);

private:
    WiFiManager* wifi_manager;
    MQTTManager* mqtt_manager;

    /**
     * Parse WiFi credentials from string
     * Format: "SSID|password"
     * @param input Input string
     * @param ssid Output SSID
     * @param password Output password
     * @return true if parsing succeeded
     */
    bool parse_wifi_credentials(const String& input, String& ssid, String& password);

    /**
     * Parse MQTT config from string
     * Format: "host:port|username|password"
     * @param input Input string
     * @param broker Output broker address
     * @param port Output port
     * @param username Output username
     * @param password Output password
     * @return true if parsing succeeded
     */
    bool parse_mqtt_config(const String& input, String& broker, uint16_t& port,
                          String& username, String& password);

    /**
     * Execute a control command
     * @param cmd Command to execute
     * @return true if command executed successfully
     */
    bool execute_control_command(NetworkControlCommand cmd);
};
