#include "wifi_manager.h"

WiFiManager::WiFiManager()
    : preferences(nullptr)
    , enabled(false)
    , status(WiFiConnectionStatus::DISABLED)
    , last_connection_attempt(0)
    , reconnect_interval(WIFI_RECONNECT_INTERVAL_MS)
    , reconnect_attempts(0)
    , status_callback(nullptr) {
}

WiFiManager::~WiFiManager() {
    if (enabled) {
        disable();
    }
}

void WiFiManager::init(Preferences* prefs) {
    preferences = prefs;

    // Load credentials and enabled state from NVS
    if (preferences) {
        enabled = preferences->getBool("wifi_enabled", false);
        load_credentials();

        Serial.println("[WiFi] Initialized");
        Serial.printf("[WiFi] Enabled: %s\n", enabled ? "true" : "false");
        Serial.printf("[WiFi] Has credentials: %s\n", has_credentials() ? "true" : "false");

        // If enabled and has credentials, attempt connection
        if (enabled && has_credentials()) {
            enable();
        }
    } else {
        Serial.println("[WiFi] Error: Preferences not provided");
    }
}

void WiFiManager::enable() {
    if (enabled && status != WiFiConnectionStatus::DISABLED) {
        Serial.println("[WiFi] Already enabled");
        return;
    }

    if (!has_credentials()) {
        Serial.println("[WiFi] Error: No credentials configured");
        update_status(WiFiConnectionStatus::ERROR);
        return;
    }

    Serial.println("[WiFi] Enabling...");
    enabled = true;

    // Save enabled state
    if (preferences) {
        preferences->putBool("wifi_enabled", true);
    }

    // Set WiFi mode to station
    WiFi.mode(WIFI_STA);
    WiFi.setAutoReconnect(false); // We handle reconnection manually

    // Reset reconnection state
    reconnect_attempts = 0;
    reconnect_interval = WIFI_RECONNECT_INTERVAL_MS;

    // Start connection attempt
    connect();
}

void WiFiManager::disable() {
    if (!enabled) {
        Serial.println("[WiFi] Already disabled");
        return;
    }

    Serial.println("[WiFi] Disabling...");
    enabled = false;

    // Save disabled state
    if (preferences) {
        preferences->putBool("wifi_enabled", false);
    }

    // Disconnect and turn off WiFi
    WiFi.disconnect(true);
    WiFi.mode(WIFI_OFF);

    update_status(WiFiConnectionStatus::DISABLED);
}

void WiFiManager::handle() {
    if (!enabled) {
        return;
    }

    // Check current WiFi status
    wl_status_t wl_status = WiFi.status();

    switch (status) {
        case WiFiConnectionStatus::DISABLED:
            // Should not be here if enabled
            break;

        case WiFiConnectionStatus::CONNECTING:
            // Check if connection succeeded or timed out
            if (wl_status == WL_CONNECTED) {
                update_status(WiFiConnectionStatus::CONNECTED);
                log_status(WiFiConnectionStatus::CONNECTED, WiFi.localIP().toString().c_str());
                reconnect_attempts = 0;
                reconnect_interval = WIFI_RECONNECT_INTERVAL_MS;
            } else if (millis() - last_connection_attempt > WIFI_CONNECTION_TIMEOUT_MS) {
                Serial.println("[WiFi] Connection timeout");
                WiFi.disconnect();
                update_status(WiFiConnectionStatus::DISCONNECTED);
                handle_reconnect();
            }
            break;

        case WiFiConnectionStatus::CONNECTED:
            // Check if connection was lost
            if (wl_status != WL_CONNECTED) {
                Serial.println("[WiFi] Connection lost");
                update_status(WiFiConnectionStatus::DISCONNECTED);
                reconnect_attempts = 0;
                reconnect_interval = WIFI_RECONNECT_INTERVAL_MS;
                handle_reconnect();
            }
            break;

        case WiFiConnectionStatus::DISCONNECTED:
        case WiFiConnectionStatus::ERROR:
            // Attempt reconnection
            handle_reconnect();
            break;
    }
}

bool WiFiManager::set_credentials(const char* new_ssid, const char* new_password) {
    if (!new_ssid || strlen(new_ssid) == 0) {
        Serial.println("[WiFi] Error: Invalid SSID");
        return false;
    }

    if (!new_password || strlen(new_password) == 0) {
        Serial.println("[WiFi] Error: Invalid password");
        return false;
    }

    // Validate lengths
    if (strlen(new_ssid) > WIFI_MAX_SSID_LENGTH) {
        Serial.println("[WiFi] Error: SSID too long");
        return false;
    }

    if (strlen(new_password) > WIFI_MAX_PASSWORD_LENGTH) {
        Serial.println("[WiFi] Error: Password too long");
        return false;
    }

    Serial.printf("[WiFi] Setting credentials for SSID: %s\n", new_ssid);

    ssid = String(new_ssid);
    password = String(new_password);

    // Save to NVS
    if (preferences) {
        preferences->putString("wifi_ssid", ssid);
        preferences->putString("wifi_password", password);
        Serial.println("[WiFi] Credentials saved to NVS");
    }

    return true;
}

String WiFiManager::get_ip_address() const {
    if (status == WiFiConnectionStatus::CONNECTED) {
        return WiFi.localIP().toString();
    }
    return String("");
}

int WiFiManager::get_rssi() const {
    if (status == WiFiConnectionStatus::CONNECTED) {
        return WiFi.RSSI();
    }
    return 0;
}

void WiFiManager::set_status_callback(StatusCallback callback) {
    status_callback = callback;
}

bool WiFiManager::has_credentials() const {
    return ssid.length() > 0 && password.length() > 0;
}

void WiFiManager::clear_credentials() {
    Serial.println("[WiFi] Clearing credentials");

    ssid = "";
    password = "";

    if (preferences) {
        preferences->remove("wifi_ssid");
        preferences->remove("wifi_password");
    }

    if (enabled) {
        disable();
    }
}

void WiFiManager::load_credentials() {
    if (!preferences) {
        return;
    }

    ssid = preferences->getString("wifi_ssid", "");
    password = preferences->getString("wifi_password", "");

    if (has_credentials()) {
        Serial.printf("[WiFi] Loaded credentials for SSID: %s\n", ssid.c_str());
    } else {
        Serial.println("[WiFi] No credentials found in NVS");
    }
}

void WiFiManager::connect() {
    if (!has_credentials()) {
        Serial.println("[WiFi] Cannot connect: No credentials");
        update_status(WiFiConnectionStatus::ERROR);
        return;
    }

    Serial.printf("[WiFi] Connecting to: %s\n", ssid.c_str());
    update_status(WiFiConnectionStatus::CONNECTING);

    WiFi.begin(ssid.c_str(), password.c_str());
    last_connection_attempt = millis();
}

void WiFiManager::handle_reconnect() {
    // Check if we've exceeded max attempts
    if (reconnect_attempts >= WIFI_MAX_RECONNECT_ATTEMPTS) {
        if (status != WiFiConnectionStatus::ERROR) {
            Serial.println("[WiFi] Max reconnect attempts reached");
            update_status(WiFiConnectionStatus::ERROR);
        }
        return;
    }

    // Check if enough time has passed since last attempt
    if (millis() - last_connection_attempt < reconnect_interval) {
        return;
    }

    // Increment attempts and apply exponential backoff
    reconnect_attempts++;
    reconnect_interval = min(reconnect_interval * 2, WIFI_MAX_RECONNECT_INTERVAL_MS);

    Serial.printf("[WiFi] Reconnect attempt %d/%d (next in %lums)\n",
                  reconnect_attempts, WIFI_MAX_RECONNECT_ATTEMPTS, reconnect_interval);

    connect();
}

void WiFiManager::update_status(WiFiConnectionStatus new_status) {
    if (status != new_status) {
        status = new_status;
        log_status(new_status);

        if (status_callback) {
            status_callback(new_status);
        }
    }
}

void WiFiManager::log_status(WiFiConnectionStatus s, const char* extra) {
    const char* status_str = "UNKNOWN";
    switch (s) {
        case WiFiConnectionStatus::DISABLED:
            status_str = "DISABLED";
            break;
        case WiFiConnectionStatus::DISCONNECTED:
            status_str = "DISCONNECTED";
            break;
        case WiFiConnectionStatus::CONNECTING:
            status_str = "CONNECTING";
            break;
        case WiFiConnectionStatus::CONNECTED:
            status_str = "CONNECTED";
            break;
        case WiFiConnectionStatus::ERROR:
            status_str = "ERROR";
            break;
    }

    if (extra) {
        Serial.printf("[WiFi] Status: %s (%s)\n", status_str, extra);
    } else {
        Serial.printf("[WiFi] Status: %s\n", status_str);
    }
}
