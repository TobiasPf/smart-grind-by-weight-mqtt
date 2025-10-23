#include "network_config_service.h"

NetworkConfigService::NetworkConfigService()
    : wifi_manager(nullptr)
    , mqtt_manager(nullptr) {
}

NetworkConfigService::~NetworkConfigService() {
}

void NetworkConfigService::init(WiFiManager* wifi, MQTTManager* mqtt) {
    wifi_manager = wifi;
    mqtt_manager = mqtt;
    Serial.println("[NetworkConfig] Service initialized");
}

bool NetworkConfigService::handle_wifi_credentials_write(const uint8_t* data, size_t length) {
    if (!wifi_manager) {
        Serial.println("[NetworkConfig] Error: WiFiManager not initialized");
        return false;
    }

    if (!data || length == 0 || length > BLE_NETWORK_MAX_PAYLOAD_BYTES) {
        Serial.println("[NetworkConfig] Error: Invalid WiFi credentials data");
        return false;
    }

    // Convert to string
    String input = String((const char*)data, length);
    Serial.printf("[NetworkConfig] Received WiFi credentials: %s\n", input.c_str());

    // Parse credentials
    String ssid, password;
    if (!parse_wifi_credentials(input, ssid, password)) {
        Serial.println("[NetworkConfig] Error: Failed to parse WiFi credentials");
        return false;
    }

    // Set credentials
    if (!wifi_manager->set_credentials(ssid.c_str(), password.c_str())) {
        Serial.println("[NetworkConfig] Error: Failed to set WiFi credentials");
        return false;
    }

    Serial.println("[NetworkConfig] WiFi credentials set successfully");
    return true;
}

bool NetworkConfigService::handle_mqtt_config_write(const uint8_t* data, size_t length) {
    if (!mqtt_manager) {
        Serial.println("[NetworkConfig] Error: MQTTManager not initialized");
        return false;
    }

    if (!data || length == 0 || length > BLE_NETWORK_MAX_PAYLOAD_BYTES) {
        Serial.println("[NetworkConfig] Error: Invalid MQTT config data");
        return false;
    }

    // Convert to string
    String input = String((const char*)data, length);
    Serial.printf("[NetworkConfig] Received MQTT config: %s\n", input.c_str());

    // Parse config
    String broker, username, password;
    uint16_t port;
    if (!parse_mqtt_config(input, broker, port, username, password)) {
        Serial.println("[NetworkConfig] Error: Failed to parse MQTT config");
        return false;
    }

    // Set config
    if (!mqtt_manager->set_broker_config(broker.c_str(), port, username.c_str(), password.c_str())) {
        Serial.println("[NetworkConfig] Error: Failed to set MQTT config");
        return false;
    }

    Serial.println("[NetworkConfig] MQTT config set successfully");
    return true;
}

bool NetworkConfigService::handle_control_write(const uint8_t* data, size_t length) {
    if (!data || length == 0) {
        Serial.println("[NetworkConfig] Error: Invalid control data");
        return false;
    }

    NetworkControlCommand cmd = static_cast<NetworkControlCommand>(data[0]);
    Serial.printf("[NetworkConfig] Received control command: 0x%02X\n", data[0]);

    return execute_control_command(cmd);
}

bool NetworkConfigService::get_status_json(String& out) {
    if (!wifi_manager || !mqtt_manager) {
        Serial.println("[NetworkConfig] Error: Managers not initialized");
        return false;
    }

    JsonDocument doc;

    // WiFi status
    JsonObject wifi = doc["wifi"].to<JsonObject>();
    wifi["enabled"] = wifi_manager->is_enabled();
    wifi["connected"] = wifi_manager->is_connected();
    wifi["has_credentials"] = wifi_manager->has_credentials();

    if (wifi_manager->has_credentials()) {
        wifi["ssid"] = wifi_manager->get_ssid();
    }

    if (wifi_manager->is_connected()) {
        wifi["ip"] = wifi_manager->get_ip_address();
        wifi["rssi"] = wifi_manager->get_rssi();
    }

    // WiFi status string
    switch (wifi_manager->get_status()) {
        case WiFiConnectionStatus::Disabled:
            wifi["status"] = "disabled";
            break;
        case WiFiConnectionStatus::Disconnected:
            wifi["status"] = "disconnected";
            break;
        case WiFiConnectionStatus::Connecting:
            wifi["status"] = "connecting";
            break;
        case WiFiConnectionStatus::Connected:
            wifi["status"] = "connected";
            break;
        case WiFiConnectionStatus::Failed:
            wifi["status"] = "error";
            break;
    }

    // MQTT status
    JsonObject mqtt = doc["mqtt"].to<JsonObject>();
    mqtt["enabled"] = mqtt_manager->is_enabled();
    mqtt["connected"] = mqtt_manager->is_connected();
    mqtt["has_config"] = mqtt_manager->has_broker_config();

    if (mqtt_manager->has_broker_config()) {
        mqtt["broker"] = mqtt_manager->get_broker();
        mqtt["port"] = mqtt_manager->get_port();
    }

    mqtt["pending_publishes"] = mqtt_manager->get_pending_count();

    // MQTT status string
    switch (mqtt_manager->get_status()) {
        case MQTTConnectionStatus::Disabled:
            mqtt["status"] = "disabled";
            break;
        case MQTTConnectionStatus::Disconnected:
            mqtt["status"] = "disconnected";
            break;
        case MQTTConnectionStatus::Connecting:
            mqtt["status"] = "connecting";
            break;
        case MQTTConnectionStatus::Connected:
            mqtt["status"] = "connected";
            break;
        case MQTTConnectionStatus::Failed:
            mqtt["status"] = "error";
            break;
    }

    // Serialize to string
    serializeJson(doc, out);

    return true;
}

size_t NetworkConfigService::handle_status_read(uint8_t* out, size_t max_len) {
    String json_status;
    if (!get_status_json(json_status)) {
        return 0;
    }

    size_t len = min(json_status.length(), max_len);
    memcpy(out, json_status.c_str(), len);
    return len;
}

bool NetworkConfigService::parse_wifi_credentials(const String& input, String& ssid, String& password) {
    // Format: "SSID|password"
    int separator = input.indexOf('|');
    if (separator <= 0) {
        Serial.println("[NetworkConfig] Error: Invalid WiFi format (missing separator)");
        return false;
    }

    ssid = input.substring(0, separator);
    password = input.substring(separator + 1);

    // Trim whitespace
    ssid.trim();
    password.trim();

    if (ssid.length() == 0) {
        Serial.println("[NetworkConfig] Error: Empty SSID");
        return false;
    }

    if (password.length() == 0) {
        Serial.println("[NetworkConfig] Error: Empty password");
        return false;
    }

    return true;
}

bool NetworkConfigService::parse_mqtt_config(const String& input, String& broker, uint16_t& port,
                                             String& username, String& password) {
    // Format: "host:port|username|password"
    // Example: "mqtt.example.com:1883|user|pass" or "mqtt.example.com:1883||" for no auth

    // Find main separator
    int separator = input.indexOf('|');
    if (separator <= 0) {
        Serial.println("[NetworkConfig] Error: Invalid MQTT format (missing separator)");
        return false;
    }

    // Parse broker and port
    String broker_part = input.substring(0, separator);
    int colon = broker_part.indexOf(':');
    if (colon <= 0) {
        Serial.println("[NetworkConfig] Error: Invalid MQTT format (missing port)");
        return false;
    }

    broker = broker_part.substring(0, colon);
    String port_str = broker_part.substring(colon + 1);
    port = port_str.toInt();

    if (port == 0) {
        Serial.println("[NetworkConfig] Error: Invalid port number");
        return false;
    }

    // Parse credentials (optional)
    String creds_part = input.substring(separator + 1);
    int creds_separator = creds_part.indexOf('|');

    if (creds_separator >= 0) {
        username = creds_part.substring(0, creds_separator);
        password = creds_part.substring(creds_separator + 1);
    } else {
        // No password separator, treat entire remainder as username
        username = creds_part;
        password = "";
    }

    // Trim whitespace
    broker.trim();
    username.trim();
    password.trim();

    if (broker.length() == 0) {
        Serial.println("[NetworkConfig] Error: Empty broker address");
        return false;
    }

    Serial.printf("[NetworkConfig] Parsed MQTT: broker=%s, port=%d, username=%s\n",
                  broker.c_str(), port, username.length() > 0 ? username.c_str() : "(none)");

    return true;
}

bool NetworkConfigService::execute_control_command(NetworkControlCommand cmd) {
    if (!wifi_manager || !mqtt_manager) {
        Serial.println("[NetworkConfig] Error: Managers not initialized");
        return false;
    }

    switch (cmd) {
        case NetworkControlCommand::ENABLE_WIFI:
            Serial.println("[NetworkConfig] Enabling WiFi...");
            wifi_manager->enable();
            return true;

        case NetworkControlCommand::DISABLE_WIFI:
            Serial.println("[NetworkConfig] Disabling WiFi...");
            wifi_manager->disable();
            return true;

        case NetworkControlCommand::ENABLE_MQTT:
            Serial.println("[NetworkConfig] Enabling MQTT...");
            mqtt_manager->enable();
            return true;

        case NetworkControlCommand::DISABLE_MQTT:
            Serial.println("[NetworkConfig] Disabling MQTT...");
            mqtt_manager->disable();
            return true;

        case NetworkControlCommand::TEST_CONNECTION:
            Serial.println("[NetworkConfig] Testing connection...");
            return mqtt_manager->test_connection();

        case NetworkControlCommand::GET_STATUS:
            Serial.println("[NetworkConfig] Status requested (will be read via status characteristic)");
            return true;

        default:
            Serial.printf("[NetworkConfig] Error: Unknown command 0x%02X\n", static_cast<uint8_t>(cmd));
            return false;
    }
}
