#include "mqtt_manager.h"

MQTTManager::MQTTManager()
    : preferences(nullptr)
    , mqtt_client(wifi_client)
    , port(MQTT_DEFAULT_PORT)
    , enabled(false)
    , status(MQTTConnectionStatus::DISABLED)
    , last_connection_attempt(0)
    , reconnect_interval(MQTT_RECONNECT_INTERVAL_MS)
    , reconnect_attempts(0)
    , status_callback(nullptr)
    , publish_callback(nullptr) {

    // Set MQTT callback (required by PubSubClient, even if we don't subscribe)
    mqtt_client.setCallback(mqtt_callback);
}

MQTTManager::~MQTTManager() {
    if (enabled) {
        disable();
    }
}

void MQTTManager::init(Preferences* prefs) {
    preferences = prefs;

    // Load configuration from NVS
    if (preferences) {
        enabled = preferences->getBool("mqtt_enabled", false);
        load_broker_config();

        Serial.println("[MQTT] Initialized");
        Serial.printf("[MQTT] Enabled: %s\n", enabled ? "true" : "false");
        Serial.printf("[MQTT] Has broker config: %s\n", has_broker_config() ? "true" : "false");

        // Note: We don't auto-enable MQTT here, as it requires WiFi to be connected first
        // The network task will call enable() when WiFi is ready
    } else {
        Serial.println("[MQTT] Error: Preferences not provided");
    }
}

void MQTTManager::enable() {
    if (enabled && status != MQTTConnectionStatus::DISABLED) {
        Serial.println("[MQTT] Already enabled");
        return;
    }

    if (!has_broker_config()) {
        Serial.println("[MQTT] Error: No broker configured");
        update_status(MQTTConnectionStatus::ERROR);
        return;
    }

    // Check if WiFi is connected
    if (WiFi.status() != WL_CONNECTED) {
        Serial.println("[MQTT] Error: WiFi not connected");
        update_status(MQTTConnectionStatus::ERROR);
        return;
    }

    Serial.println("[MQTT] Enabling...");
    enabled = true;

    // Save enabled state
    if (preferences) {
        preferences->putBool("mqtt_enabled", true);
    }

    // Configure MQTT client
    mqtt_client.setServer(broker.c_str(), port);
    mqtt_client.setKeepAlive(MQTT_KEEP_ALIVE_SEC);
    mqtt_client.setSocketTimeout(MQTT_CONNECTION_TIMEOUT_MS / 1000);

    // Reset reconnection state
    reconnect_attempts = 0;
    reconnect_interval = MQTT_RECONNECT_INTERVAL_MS;

    // Start connection attempt
    connect();
}

void MQTTManager::disable() {
    if (!enabled) {
        Serial.println("[MQTT] Already disabled");
        return;
    }

    Serial.println("[MQTT] Disabling...");
    enabled = false;

    // Save disabled state
    if (preferences) {
        preferences->putBool("mqtt_enabled", false);
    }

    // Disconnect from broker
    if (mqtt_client.connected()) {
        mqtt_client.disconnect();
    }

    update_status(MQTTConnectionStatus::DISABLED);

    // Clear publish queue
    while (!publish_queue.empty()) {
        publish_queue.pop();
    }
}

void MQTTManager::handle() {
    if (!enabled) {
        return;
    }

    // Check if WiFi is still connected
    if (WiFi.status() != WL_CONNECTED) {
        if (status != MQTTConnectionStatus::ERROR) {
            Serial.println("[MQTT] WiFi disconnected");
            update_status(MQTTConnectionStatus::ERROR);
        }
        return;
    }

    // Process MQTT client loop (handle keep-alive, etc.)
    if (mqtt_client.connected()) {
        mqtt_client.loop();
    }

    // Check connection status
    switch (status) {
        case MQTTConnectionStatus::DISABLED:
            // Should not be here if enabled
            break;

        case MQTTConnectionStatus::CONNECTING:
            // Check if connection succeeded or timed out
            if (mqtt_client.connected()) {
                update_status(MQTTConnectionStatus::CONNECTED);
                log_status(MQTTConnectionStatus::CONNECTED, broker.c_str());
                reconnect_attempts = 0;
                reconnect_interval = MQTT_RECONNECT_INTERVAL_MS;

                // Process any pending publishes
                process_publish_queue();
            } else if (millis() - last_connection_attempt > MQTT_CONNECTION_TIMEOUT_MS) {
                Serial.println("[MQTT] Connection timeout");
                update_status(MQTTConnectionStatus::DISCONNECTED);
                handle_reconnect();
            }
            break;

        case MQTTConnectionStatus::CONNECTED:
            // Check if connection was lost
            if (!mqtt_client.connected()) {
                Serial.println("[MQTT] Connection lost");
                update_status(MQTTConnectionStatus::DISCONNECTED);
                reconnect_attempts = 0;
                reconnect_interval = MQTT_RECONNECT_INTERVAL_MS;
                handle_reconnect();
            } else {
                // Process pending publishes
                process_publish_queue();
            }
            break;

        case MQTTConnectionStatus::DISCONNECTED:
        case MQTTConnectionStatus::ERROR:
            // Attempt reconnection
            handle_reconnect();
            break;
    }
}

bool MQTTManager::set_broker_config(const char* new_broker, uint16_t new_port,
                                    const char* new_username, const char* new_password) {
    if (!new_broker || strlen(new_broker) == 0) {
        Serial.println("[MQTT] Error: Invalid broker address");
        return false;
    }

    if (new_port == 0) {
        Serial.println("[MQTT] Error: Invalid port");
        return false;
    }

    // Validate lengths
    if (strlen(new_broker) > MQTT_MAX_BROKER_LENGTH) {
        Serial.println("[MQTT] Error: Broker address too long");
        return false;
    }

    if (new_username && strlen(new_username) > MQTT_MAX_USERNAME_LENGTH) {
        Serial.println("[MQTT] Error: Username too long");
        return false;
    }

    if (new_password && strlen(new_password) > MQTT_MAX_PASSWORD_LENGTH) {
        Serial.println("[MQTT] Error: Password too long");
        return false;
    }

    Serial.printf("[MQTT] Setting broker: %s:%d\n", new_broker, new_port);

    broker = String(new_broker);
    port = new_port;
    username = new_username ? String(new_username) : String("");
    password = new_password ? String(new_password) : String("");

    // Save to NVS
    if (preferences) {
        preferences->putString("mqtt_broker", broker);
        preferences->putUShort("mqtt_port", port);
        preferences->putString("mqtt_username", username);
        preferences->putString("mqtt_password", password);
        Serial.println("[MQTT] Broker configuration saved to NVS");
    }

    return true;
}

MQTTPublishResult MQTTManager::publish_session(const GrindSession* session) {
    if (!session) {
        Serial.println("[MQTT] Error: Null session pointer");
        return MQTTPublishResult::FAILED;
    }

    if (!enabled) {
        Serial.println("[MQTT] Error: MQTT not enabled");
        return MQTTPublishResult::FAILED;
    }

    // Serialize session to JSON
    String json_payload;
    if (!GrindSessionSerializer::serialize_session_to_json(session, json_payload)) {
        Serial.println("[MQTT] Error: Failed to serialize session to JSON");
        return MQTTPublishResult::FAILED;
    }

    // Build topic
    String topic = build_session_topic(session->session_id);

    Serial.printf("[MQTT] Publishing session %lu to %s\n", session->session_id, topic.c_str());
    Serial.printf("[MQTT] Payload size: %d bytes\n", json_payload.length());

    // Attempt to publish
    if (status == MQTTConnectionStatus::CONNECTED) {
        bool success = publish(topic.c_str(), json_payload.c_str(), MQTT_RETAIN_SESSIONS);

        if (success) {
            Serial.printf("[MQTT] Published session %lu successfully\n", session->session_id);
            if (publish_callback) {
                publish_callback(session->session_id, MQTTPublishResult::SUCCESS);
            }
            return MQTTPublishResult::SUCCESS;
        } else {
            Serial.printf("[MQTT] Failed to publish session %lu\n", session->session_id);
        }
    }

    // Queue for retry if not connected or publish failed
    if (publish_queue.size() < MQTT_MAX_FAILED_PUBLISH_QUEUE) {
        Serial.printf("[MQTT] Queuing session %lu for retry\n", session->session_id);
        publish_queue.emplace(topic, json_payload);
        if (publish_callback) {
            publish_callback(session->session_id, MQTTPublishResult::QUEUED);
        }
        return MQTTPublishResult::QUEUED;
    } else {
        Serial.printf("[MQTT] Queue full, dropping session %lu\n", session->session_id);
        if (publish_callback) {
            publish_callback(session->session_id, MQTTPublishResult::FAILED);
        }
        return MQTTPublishResult::FAILED;
    }
}

void MQTTManager::set_status_callback(StatusCallback callback) {
    status_callback = callback;
}

void MQTTManager::set_publish_callback(PublishCallback callback) {
    publish_callback = callback;
}

bool MQTTManager::has_broker_config() const {
    return broker.length() > 0 && port > 0;
}

void MQTTManager::clear_broker_config() {
    Serial.println("[MQTT] Clearing broker configuration");

    broker = "";
    port = MQTT_DEFAULT_PORT;
    username = "";
    password = "";

    if (preferences) {
        preferences->remove("mqtt_broker");
        preferences->remove("mqtt_port");
        preferences->remove("mqtt_username");
        preferences->remove("mqtt_password");
    }

    if (enabled) {
        disable();
    }
}

bool MQTTManager::test_connection() {
    if (status != MQTTConnectionStatus::CONNECTED) {
        Serial.println("[MQTT] Cannot test: Not connected");
        return false;
    }

    // Publish a test message to the status topic
    String device_id = GrindSessionSerializer::get_device_id();
    char topic[MQTT_MAX_TOPIC_LENGTH];
    snprintf(topic, sizeof(topic), MQTT_WILL_TOPIC_PATTERN, device_id.c_str());

    Serial.printf("[MQTT] Testing connection with message to %s\n", topic);
    bool success = publish(topic, MQTT_ONLINE_MESSAGE, false);

    if (success) {
        Serial.println("[MQTT] Test publish succeeded");
    } else {
        Serial.println("[MQTT] Test publish failed");
    }

    return success;
}

void MQTTManager::load_broker_config() {
    if (!preferences) {
        return;
    }

    broker = preferences->getString("mqtt_broker", "");
    port = preferences->getUShort("mqtt_port", MQTT_DEFAULT_PORT);
    username = preferences->getString("mqtt_username", "");
    password = preferences->getString("mqtt_password", "");

    if (has_broker_config()) {
        Serial.printf("[MQTT] Loaded broker: %s:%d\n", broker.c_str(), port);
    } else {
        Serial.println("[MQTT] No broker configuration found in NVS");
    }
}

void MQTTManager::connect() {
    if (!has_broker_config()) {
        Serial.println("[MQTT] Cannot connect: No broker configured");
        update_status(MQTTConnectionStatus::ERROR);
        return;
    }

    if (WiFi.status() != WL_CONNECTED) {
        Serial.println("[MQTT] Cannot connect: WiFi not connected");
        update_status(MQTTConnectionStatus::ERROR);
        return;
    }

    Serial.printf("[MQTT] Connecting to broker: %s:%d\n", broker.c_str(), port);
    update_status(MQTTConnectionStatus::CONNECTING);

    // Build client ID from device ID
    String client_id = GrindSessionSerializer::get_device_id();

    // Build will topic
    char will_topic[MQTT_MAX_TOPIC_LENGTH];
    snprintf(will_topic, sizeof(will_topic), MQTT_WILL_TOPIC_PATTERN, client_id.c_str());

    // Attempt connection
    bool connected = false;
    if (username.length() > 0) {
        // Connect with authentication and last will
        connected = mqtt_client.connect(client_id.c_str(),
                                       username.c_str(),
                                       password.c_str(),
                                       will_topic,
                                       MQTT_QOS_LEVEL,
                                       true, // retain will message
                                       MQTT_WILL_MESSAGE);
    } else {
        // Connect without authentication, with last will
        connected = mqtt_client.connect(client_id.c_str(),
                                       will_topic,
                                       MQTT_QOS_LEVEL,
                                       true, // retain will message
                                       MQTT_WILL_MESSAGE);
    }

    last_connection_attempt = millis();

    if (connected) {
        // Publish online status
        publish(will_topic, MQTT_ONLINE_MESSAGE, true);
    }
}

void MQTTManager::handle_reconnect() {
    // Check if enough time has passed since last attempt
    if (millis() - last_connection_attempt < reconnect_interval) {
        return;
    }

    // Increment attempts and apply exponential backoff
    reconnect_attempts++;
    reconnect_interval = min(reconnect_interval * 2, MQTT_MAX_RECONNECT_INTERVAL_MS);

    Serial.printf("[MQTT] Reconnect attempt %d (next in %lums)\n",
                  reconnect_attempts, reconnect_interval);

    connect();
}

void MQTTManager::process_publish_queue() {
    if (status != MQTTConnectionStatus::CONNECTED) {
        return;
    }

    // Process up to 3 pending publishes per cycle to avoid blocking
    const int max_per_cycle = 3;
    int processed = 0;

    while (!publish_queue.empty() && processed < max_per_cycle) {
        PendingPublish& pending = publish_queue.front();

        Serial.printf("[MQTT] Retrying queued publish to %s\n", pending.topic.c_str());

        bool success = publish(pending.topic.c_str(), pending.payload.c_str(), MQTT_RETAIN_SESSIONS);

        if (success) {
            Serial.println("[MQTT] Queued publish succeeded");
            publish_queue.pop();
        } else {
            // Increment retry count
            pending.retry_count++;

            if (pending.retry_count >= 3) {
                Serial.println("[MQTT] Max retries reached, dropping publish");
                publish_queue.pop();
            } else {
                Serial.printf("[MQTT] Retry %d failed, keeping in queue\n", pending.retry_count);
                // Move to back of queue
                publish_queue.push(pending);
                publish_queue.pop();
            }
        }

        processed++;
    }
}

bool MQTTManager::publish(const char* topic, const char* payload, bool retain) {
    if (!mqtt_client.connected()) {
        return false;
    }

    // Check buffer size
    if (strlen(payload) > mqtt_client.getBufferSize()) {
        Serial.printf("[MQTT] Error: Payload too large (%d bytes, max %d)\n",
                     strlen(payload), mqtt_client.getBufferSize());
        return false;
    }

    return mqtt_client.publish(topic, payload, retain);
}

void MQTTManager::update_status(MQTTConnectionStatus new_status) {
    if (status != new_status) {
        status = new_status;
        log_status(new_status);

        if (status_callback) {
            status_callback(new_status);
        }
    }
}

void MQTTManager::log_status(MQTTConnectionStatus s, const char* extra) {
    const char* status_str = "UNKNOWN";
    switch (s) {
        case MQTTConnectionStatus::DISABLED:
            status_str = "DISABLED";
            break;
        case MQTTConnectionStatus::DISCONNECTED:
            status_str = "DISCONNECTED";
            break;
        case MQTTConnectionStatus::CONNECTING:
            status_str = "CONNECTING";
            break;
        case MQTTConnectionStatus::CONNECTED:
            status_str = "CONNECTED";
            break;
        case MQTTConnectionStatus::ERROR:
            status_str = "ERROR";
            break;
    }

    if (extra) {
        Serial.printf("[MQTT] Status: %s (%s)\n", status_str, extra);
    } else {
        Serial.printf("[MQTT] Status: %s\n", status_str);
    }
}

String MQTTManager::build_session_topic(uint32_t session_id) {
    String device_id = GrindSessionSerializer::get_device_id();
    char topic[MQTT_MAX_TOPIC_LENGTH];
    snprintf(topic, sizeof(topic), MQTT_TOPIC_PATTERN, device_id.c_str(), session_id);
    return String(topic);
}

void MQTTManager::mqtt_callback(char* topic, byte* payload, unsigned int length) {
    // We don't subscribe to anything, so this should never be called
    // But it's required by PubSubClient
}
