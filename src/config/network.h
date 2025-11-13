#pragma once

//==============================================================================
// NETWORK CONFIGURATION
//==============================================================================
// This file contains WiFi and MQTT network configuration constants for
// connecting the ESP32 coffee grinder to a network and publishing grind
// session data to an MQTT broker.

//------------------------------------------------------------------------------
// UART GATEWAY ENABLE/DISABLE
//------------------------------------------------------------------------------
// Set to 1 to enable UART gateway (requires ESP32-C3 gateway board)
// Set to 0 to disable network features completely (standalone operation)
#define ENABLE_UART_GATEWAY 0

//------------------------------------------------------------------------------
// WIFI CONFIGURATION
//------------------------------------------------------------------------------
#define WIFI_MAX_SSID_LENGTH 32                                            // Maximum SSID length
#define WIFI_MAX_PASSWORD_LENGTH 64                                        // Maximum password length
#define WIFI_CONNECTION_TIMEOUT_MS 10000                                   // WiFi connection timeout (10s)
#define WIFI_RECONNECT_INTERVAL_MS 5000                                    // Base reconnect interval (5s)
#define WIFI_MAX_RECONNECT_INTERVAL_MS 30000                               // Maximum reconnect interval (30s with exponential backoff)
#define WIFI_MAX_RECONNECT_ATTEMPTS 3                                      // Max reconnect attempts before giving up

//------------------------------------------------------------------------------
// MQTT CONFIGURATION
//------------------------------------------------------------------------------
#define MQTT_MAX_BROKER_LENGTH 128                                         // Maximum broker address length
#define MQTT_MAX_USERNAME_LENGTH 64                                        // Maximum username length
#define MQTT_MAX_PASSWORD_LENGTH 64                                        // Maximum password length
#define MQTT_MAX_TOPIC_LENGTH 128                                          // Maximum topic length
#define MQTT_DEFAULT_PORT 1883                                             // Default MQTT port (non-SSL)
#define MQTT_CONNECTION_TIMEOUT_MS 10000                                   // MQTT connection timeout (10s)
#define MQTT_KEEP_ALIVE_SEC 60                                             // MQTT keep-alive interval (60s)
#define MQTT_RECONNECT_INTERVAL_MS 5000                                    // Base reconnect interval (5s)
#define MQTT_MAX_RECONNECT_INTERVAL_MS 30000                               // Maximum reconnect interval (30s with exponential backoff)
#define MQTT_PUBLISH_TIMEOUT_MS 5000                                       // Publish operation timeout (5s)
#define MQTT_MAX_FAILED_PUBLISH_QUEUE 10                                   // Maximum queued failed publishes

// MQTT Topic Patterns
#define MQTT_TOPIC_PATTERN "grinder/%s/sessions/%lu"                       // Topic format: grinder/{device_id}/sessions/{session_id}
#define MQTT_WILL_TOPIC_PATTERN "grinder/%s/status"                        // Last will topic: grinder/{device_id}/status
#define MQTT_WILL_MESSAGE "offline"                                        // Last will message
#define MQTT_ONLINE_MESSAGE "online"                                       // Online status message

// MQTT Quality of Service
#define MQTT_QOS_LEVEL 0                                                   // QoS 0: Fire and forget (can upgrade to 1 for at-least-once)
#define MQTT_RETAIN_SESSIONS true                                          // Retain session messages on broker

//------------------------------------------------------------------------------
// DEVICE IDENTIFICATION
//------------------------------------------------------------------------------
// Device ID is derived from ESP32 chip ID (unique per device)
#define NETWORK_DEVICE_ID_PREFIX "esp32"                                   // Device ID prefix
#define NETWORK_DEVICE_ID_FORMAT "esp32-%08llx"                            // Device ID format: esp32-{chip_id_hex}

//------------------------------------------------------------------------------
// JSON SERIALIZATION
//------------------------------------------------------------------------------
#define JSON_SESSION_BUFFER_SIZE 512                                       // Buffer size for JSON serialization
#define JSON_SESSION_PRETTY_PRINT false                                    // Disable pretty printing for compactness

//------------------------------------------------------------------------------
// NETWORK TASK CONFIGURATION
//------------------------------------------------------------------------------
#define SYS_TASK_NETWORK_INTERVAL_MS 500                                   // Network task update interval (2Hz)
#define SYS_TASK_NETWORK_STACK_SIZE 6144                                   // 6KB stack for WiFi/MQTT operations
#define SYS_TASK_PRIORITY_NETWORK 2                                        // Medium priority (between Bluetooth and File I/O)

// Network-to-publish queue
#define SYS_QUEUE_NETWORK_PUBLISH_SIZE 10                                  // Queue size for pending publishes
