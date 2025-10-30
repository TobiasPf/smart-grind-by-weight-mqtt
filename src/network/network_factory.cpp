#include "network_factory.h"
#include "wifi_manager.h"
#include "mqtt_manager.h"

// WiFi headers are ONLY included here, not in main.cpp
// This ensures WiFi library initialization happens after display setup

WiFiManager* NetworkFactory::create_wifi_manager() {
    return new WiFiManager();
}

MQTTManager* NetworkFactory::create_mqtt_manager() {
    return new MQTTManager();
}

void NetworkFactory::destroy_wifi_manager(WiFiManager* manager) {
    delete manager;
}

void NetworkFactory::destroy_mqtt_manager(MQTTManager* manager) {
    delete manager;
}
