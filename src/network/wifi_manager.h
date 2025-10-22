#pragma once

#include <Arduino.h>
#include <WiFi.h>
#include <Preferences.h>
#include <functional>
#include "../config/constants.h"

/**
 * WiFiConnectionStatus - Current WiFi connection state
 */
enum class WiFiConnectionStatus {
    DISABLED,       // WiFi is disabled
    DISCONNECTED,   // WiFi is enabled but not connected
    CONNECTING,     // Attempting to connect
    CONNECTED,      // Successfully connected
    ERROR          // Connection error (failed after retries)
};

/**
 * WiFiManager - Manages WiFi connection and credentials
 *
 * Handles WiFi connection state, automatic reconnection with exponential
 * backoff, and persistent storage of credentials in NVS.
 */
class WiFiManager {
public:
    using StatusCallback = std::function<void(WiFiConnectionStatus)>;

    WiFiManager();
    ~WiFiManager();

    /**
     * Initialize WiFi manager with preferences
     * @param prefs Preferences object for persistent storage
     */
    void init(Preferences* prefs);

    /**
     * Enable WiFi and attempt to connect
     * Uses stored credentials from NVS
     */
    void enable();

    /**
     * Disable WiFi and disconnect
     */
    void disable();

    /**
     * Handle periodic updates (call from network task)
     * Manages connection state and reconnection attempts
     */
    void handle();

    /**
     * Set WiFi credentials and save to NVS
     * @param ssid Network SSID
     * @param password Network password
     * @return true if credentials were saved successfully
     */
    bool set_credentials(const char* ssid, const char* password);

    /**
     * Get current connection status
     */
    WiFiConnectionStatus get_status() const { return status; }

    /**
     * Check if WiFi is enabled
     */
    bool is_enabled() const { return enabled; }

    /**
     * Check if WiFi is connected
     */
    bool is_connected() const { return status == WiFiConnectionStatus::CONNECTED; }

    /**
     * Get current SSID (returns empty string if not configured)
     */
    String get_ssid() const { return ssid; }

    /**
     * Get local IP address (returns empty string if not connected)
     */
    String get_ip_address() const;

    /**
     * Get signal strength (RSSI) in dBm
     * @return RSSI value, or 0 if not connected
     */
    int get_rssi() const;

    /**
     * Set status change callback
     * Called whenever connection status changes
     */
    void set_status_callback(StatusCallback callback);

    /**
     * Check if credentials are configured
     */
    bool has_credentials() const;

    /**
     * Clear stored credentials from NVS
     */
    void clear_credentials();

private:
    Preferences* preferences;
    String ssid;
    String password;
    bool enabled;
    WiFiConnectionStatus status;
    unsigned long last_connection_attempt;
    unsigned long reconnect_interval;
    uint8_t reconnect_attempts;
    StatusCallback status_callback;

    /**
     * Load credentials from NVS
     */
    void load_credentials();

    /**
     * Attempt to connect to WiFi
     */
    void connect();

    /**
     * Handle reconnection logic with exponential backoff
     */
    void handle_reconnect();

    /**
     * Update status and trigger callback if changed
     */
    void update_status(WiFiConnectionStatus new_status);

    /**
     * Log status change to serial
     */
    void log_status(WiFiConnectionStatus s, const char* extra = nullptr);
};
