#pragma once

#include <Arduino.h>
#include <WiFi.h>
#include <PubSubClient.h>
#include <Preferences.h>
#include <functional>
#include <queue>
#include "../config/constants.h"
#include "../logging/grind_logging.h"
#include "../logging/grind_json.h"

/**
 * MQTTConnectionStatus - Current MQTT connection state
 */
enum class MQTTConnectionStatus {
    Disabled,       // MQTT is disabled
    Disconnected,   // MQTT is enabled but not connected
    Connecting,     // Attempting to connect to broker
    Connected,      // Successfully connected to broker
    Failed          // Connection error (failed after retries)
};

/**
 * MQTTPublishResult - Result of a publish attempt
 */
enum class MQTTPublishResult {
    SUCCESS,        // Publish succeeded
    FAILED,         // Publish failed (network error, not connected)
    QUEUED         // Publish queued for retry
};

/**
 * PendingPublish - Queued publish attempt
 */
struct PendingPublish {
    String topic;
    String payload;
    uint8_t retry_count;

    PendingPublish() : retry_count(0) {}
    PendingPublish(const String& t, const String& p) : topic(t), payload(p), retry_count(0) {}
};

/**
 * MQTTManager - Manages MQTT connection and publishing
 *
 * Handles MQTT broker connection, session publishing with JSON serialization,
 * automatic reconnection with exponential backoff, and queuing of failed publishes.
 */
class MQTTManager {
public:
    using StatusCallback = std::function<void(MQTTConnectionStatus)>;
    using PublishCallback = std::function<void(uint32_t session_id, MQTTPublishResult)>;

    MQTTManager();
    ~MQTTManager();

    /**
     * Initialize MQTT manager with preferences
     * @param prefs Preferences object for persistent storage
     */
    void init(Preferences* prefs);

    /**
     * Enable MQTT (requires WiFi to be connected)
     * Uses stored broker configuration from NVS
     */
    void enable();

    /**
     * Disable MQTT and disconnect
     */
    void disable();

    /**
     * Handle periodic updates (call from network task)
     * Manages connection state, reconnection, and publish queue
     */
    void handle();

    /**
     * Set MQTT broker configuration and save to NVS
     * @param broker Broker address (hostname or IP)
     * @param port Broker port (default 1883)
     * @param username Username for authentication (empty string for no auth)
     * @param password Password for authentication
     * @return true if configuration was saved successfully
     */
    bool set_broker_config(const char* broker, uint16_t port,
                          const char* username, const char* password);

    /**
     * Publish a grind session to MQTT
     * @param session Pointer to GrindSession to publish
     * @return Result of publish attempt
     */
    MQTTPublishResult publish_session(const GrindSession* session);

    /**
     * Get current connection status
     */
    MQTTConnectionStatus get_status() const { return status; }

    /**
     * Check if MQTT is enabled
     */
    bool is_enabled() const { return enabled; }

    /**
     * Check if MQTT is connected
     */
    bool is_connected() const { return status == MQTTConnectionStatus::Connected; }

    /**
     * Get broker address
     */
    String get_broker() const { return broker; }

    /**
     * Get broker port
     */
    uint16_t get_port() const { return port; }

    /**
     * Get number of pending publishes in queue
     */
    size_t get_pending_count() const { return publish_queue.size(); }

    /**
     * Set status change callback
     * Called whenever connection status changes
     */
    void set_status_callback(StatusCallback callback);

    /**
     * Set publish result callback
     * Called when a session publish completes (success or failure)
     */
    void set_publish_callback(PublishCallback callback);

    /**
     * Check if broker is configured
     */
    bool has_broker_config() const;

    /**
     * Clear stored broker configuration from NVS
     */
    void clear_broker_config();

    /**
     * Test connection by publishing a test message
     * @return true if test publish succeeded
     */
    bool test_connection();

private:
    Preferences* preferences;
    WiFiClient wifi_client;
    PubSubClient mqtt_client;
    String broker;
    uint16_t port;
    String username;
    String password;
    bool enabled;
    MQTTConnectionStatus status;
    unsigned long last_connection_attempt;
    unsigned long reconnect_interval;
    uint8_t reconnect_attempts;
    StatusCallback status_callback;
    PublishCallback publish_callback;
    std::queue<PendingPublish> publish_queue;

    /**
     * Load broker configuration from NVS
     */
    void load_broker_config();

    /**
     * Attempt to connect to MQTT broker
     */
    void connect();

    /**
     * Handle reconnection logic with exponential backoff
     */
    void handle_reconnect();

    /**
     * Process pending publish queue
     */
    void process_publish_queue();

    /**
     * Publish a message to MQTT
     * @param topic Topic to publish to
     * @param payload Payload string
     * @param retain Retain flag
     * @return true if publish succeeded
     */
    bool publish(const char* topic, const char* payload, bool retain = false);

    /**
     * Update status and trigger callback if changed
     */
    void update_status(MQTTConnectionStatus new_status);

    /**
     * Log status change to serial
     */
    void log_status(MQTTConnectionStatus s, const char* extra = nullptr);

    /**
     * Build topic string for a session
     */
    String build_session_topic(uint32_t session_id);

    /**
     * Callback for MQTT client (required by PubSubClient)
     */
    static void mqtt_callback(char* topic, byte* payload, unsigned int length);
};
