#pragma once

// Forward declarations only - no WiFi headers included
// This prevents WiFi library initialization in main.cpp
class WiFiManager;
class MQTTManager;

/**
 * Factory for creating network managers without including WiFi headers.
 * This is crucial to avoid early WiFi library initialization which causes
 * SPI bus conflicts with the display on ESP32-S3.
 */
class NetworkFactory {
public:
    static WiFiManager* create_wifi_manager();
    static MQTTManager* create_mqtt_manager();
    static void destroy_wifi_manager(WiFiManager* manager);
    static void destroy_mqtt_manager(MQTTManager* manager);
};
