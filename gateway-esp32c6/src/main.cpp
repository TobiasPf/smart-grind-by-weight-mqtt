/**
 * ESP32-C3 SuperMini WiFi/MQTT Gateway
 *
 * Receives grind session data from ESP32-S3 via UART and publishes to MQTT broker.
 * Configurable via USB serial connection using simple text commands.
 *
 * Hardware:
 * - ESP32-C3 SuperMini
 * - UART RX on GPIO20, TX on GPIO21 (Serial1)
 * - USB Serial for configuration (Serial/CDC)
 *
 * Serial Commands:
 * - wifi ssid=<ssid> pass=<password>    Configure WiFi credentials
 * - mqtt broker=<host> port=<port> [user=<user> pass=<pass>]    Configure MQTT
 * - status                               Show current status
 * - reset                                Clear all settings
 */

#include <Arduino.h>
#include <WiFi.h>
#include <PubSubClient.h>
#include <Preferences.h>
#include <ArduinoJson.h>

// For ESP32-C3 USB CDC Serial
#if ARDUINO_USB_CDC_ON_BOOT
#include "USB.h"
#endif

// Configuration
#define UART_RX_PIN 20
#define UART_TX_PIN 21
#define UART_BAUD 115200
#define MQTT_BUFFER_SIZE 2048
#define RECONNECT_INTERVAL_MS 5000

// Global objects
HardwareSerial UartS3(1);  // UART to ESP32-S3
WiFiClient wifiClient;
PubSubClient mqttClient(wifiClient);
Preferences preferences;

// State
String wifiSSID;
String wifiPassword;
String mqttBroker;
uint16_t mqttPort = 1883;
String mqttUsername;
String mqttPassword;
String deviceID;

unsigned long lastReconnectAttempt = 0;
bool wifiConnected = false;
bool mqttConnected = false;

// Function prototypes
void loadConfig();
void saveWiFiConfig(const String& ssid, const String& pass);
void saveMQTTConfig(const String& broker, uint16_t port, const String& user, const String& pass);
void connectWiFi();
void connectMQTT();
void handleSerialConfig();
void handleUartData();
void publishSession(const JsonDocument& session);
void sendStatus();
void printStatus();

void setup() {
    // Initialize USB Serial for configuration
#if ARDUINO_USB_CDC_ON_BOOT
    // USB CDC is automatically started, just wait for it
    Serial.begin(115200);
    // Wait for USB Serial to be ready (optional, for debugging)
    unsigned long start = millis();
    while (!Serial && (millis() - start < 3000)) {
        delay(10);
    }
#else
    Serial.begin(115200);
#endif

    delay(500);
    Serial.println("\n\n=== ESP32-C3 WiFi/MQTT Gateway ===");
    Serial.println("Version: 1.0.0");
    Serial.println("Build: " __DATE__ " " __TIME__);

    // Initialize UART to ESP32-S3
    UartS3.begin(UART_BAUD, SERIAL_8N1, UART_RX_PIN, UART_TX_PIN);
    Serial.println("UART initialized: RX=" + String(UART_RX_PIN) + ", TX=" + String(UART_TX_PIN));

    // Load configuration from NVS
    preferences.begin("gateway", false);
    loadConfig();

    // Generate device ID from MAC address
    uint8_t mac[6];
    WiFi.macAddress(mac);
    deviceID = String(mac[0], HEX) + String(mac[1], HEX) + String(mac[2], HEX) +
               String(mac[3], HEX) + String(mac[4], HEX) + String(mac[5], HEX);
    deviceID.toUpperCase();
    Serial.println("Device ID: " + deviceID);

    // Configure MQTT
    mqttClient.setBufferSize(MQTT_BUFFER_SIZE);
    mqttClient.setServer(mqttBroker.c_str(), mqttPort);

    Serial.println("\nReady for commands. Type 'help' for usage.\n");
    printStatus();
}

void loop() {
    // Handle USB serial configuration commands
    if (Serial.available()) {
        handleSerialConfig();
    }

    // Handle UART data from ESP32-S3
    if (UartS3.available()) {
        handleUartData();
    }

    // Maintain WiFi connection
    if (wifiSSID.length() > 0) {
        if (WiFi.status() != WL_CONNECTED) {
            if (!wifiConnected || millis() - lastReconnectAttempt > RECONNECT_INTERVAL_MS) {
                wifiConnected = false;
                connectWiFi();
                lastReconnectAttempt = millis();
            }
        } else if (!wifiConnected) {
            wifiConnected = true;
            Serial.println("WiFi connected: " + WiFi.localIP().toString());
            sendStatus();
        }
    }

    // Maintain MQTT connection
    if (wifiConnected && mqttBroker.length() > 0) {
        if (!mqttClient.connected()) {
            if (!mqttConnected || millis() - lastReconnectAttempt > RECONNECT_INTERVAL_MS) {
                mqttConnected = false;
                connectMQTT();
                lastReconnectAttempt = millis();
            }
        } else {
            if (!mqttConnected) {
                mqttConnected = true;
                Serial.println("MQTT connected to " + mqttBroker);
                sendStatus();
            }
            mqttClient.loop();
        }
    }

    delay(10);
}

void loadConfig() {
    wifiSSID = preferences.getString("wifi_ssid", "");
    wifiPassword = preferences.getString("wifi_pass", "");
    mqttBroker = preferences.getString("mqtt_broker", "");
    mqttPort = preferences.getUShort("mqtt_port", 1883);
    mqttUsername = preferences.getString("mqtt_user", "");
    mqttPassword = preferences.getString("mqtt_pass", "");

    Serial.println("\nConfiguration loaded:");
    Serial.println("  WiFi SSID: " + (wifiSSID.length() > 0 ? wifiSSID : "(not configured)"));
    Serial.println("  MQTT Broker: " + (mqttBroker.length() > 0 ? mqttBroker + ":" + String(mqttPort) : "(not configured)"));
}

void saveWiFiConfig(const String& ssid, const String& pass) {
    preferences.putString("wifi_ssid", ssid);
    preferences.putString("wifi_pass", pass);
    wifiSSID = ssid;
    wifiPassword = pass;
    Serial.println("WiFi configuration saved");
}

void saveMQTTConfig(const String& broker, uint16_t port, const String& user, const String& pass) {
    preferences.putString("mqtt_broker", broker);
    preferences.putUShort("mqtt_port", port);
    preferences.putString("mqtt_user", user);
    preferences.putString("mqtt_pass", pass);
    mqttBroker = broker;
    mqttPort = port;
    mqttUsername = user;
    mqttPassword = pass;
    mqttClient.setServer(mqttBroker.c_str(), mqttPort);
    Serial.println("MQTT configuration saved");
}

void connectWiFi() {
    if (wifiSSID.length() == 0) return;

    Serial.print("Connecting to WiFi: " + wifiSSID + "...");
    WiFi.begin(wifiSSID.c_str(), wifiPassword.c_str());

    int attempts = 0;
    while (WiFi.status() != WL_CONNECTED && attempts < 20) {
        delay(500);
        Serial.print(".");
        attempts++;
    }

    if (WiFi.status() == WL_CONNECTED) {
        Serial.println(" Connected!");
        Serial.println("IP: " + WiFi.localIP().toString());
    } else {
        Serial.println(" Failed");
    }
}

void connectMQTT() {
    if (mqttBroker.length() == 0 || !wifiConnected) return;

    Serial.print("Connecting to MQTT: " + mqttBroker + ":" + String(mqttPort) + "...");

    String clientId = "grinder-gateway-" + deviceID;
    String willTopic = "grinder/" + deviceID + "/status";

    bool connected;
    if (mqttUsername.length() > 0) {
        connected = mqttClient.connect(clientId.c_str(), mqttUsername.c_str(),
                                      mqttPassword.c_str(), willTopic.c_str(), 0, true, "offline");
    } else {
        connected = mqttClient.connect(clientId.c_str(), willTopic.c_str(), 0, true, "offline");
    }

    if (connected) {
        Serial.println(" Connected!");
        // Publish online status
        mqttClient.publish(willTopic.c_str(), "online", true);
    } else {
        Serial.println(" Failed (state=" + String(mqttClient.state()) + ")");
    }
}

void handleSerialConfig() {
    String command = Serial.readStringUntil('\n');
    command.trim();

    if (command.length() == 0) return;

    Serial.println("> " + command);

    if (command == "help") {
        Serial.println("\nAvailable commands:");
        Serial.println("  wifi ssid=<ssid> pass=<password>");
        Serial.println("  mqtt broker=<host> port=<port> [user=<user> pass=<pass>]");
        Serial.println("  status");
        Serial.println("  reset");
        Serial.println("  help");

    } else if (command == "status") {
        printStatus();

    } else if (command == "reset") {
        preferences.clear();
        Serial.println("All settings cleared. Restarting...");
        delay(1000);
        ESP.restart();

    } else if (command.startsWith("wifi ")) {
        // Parse: wifi ssid=MyNetwork pass=password123
        String params = command.substring(5);
        int ssidIdx = params.indexOf("ssid=");
        int passIdx = params.indexOf("pass=");

        if (ssidIdx >= 0 && passIdx >= 0) {
            String ssid = params.substring(ssidIdx + 5, passIdx);
            ssid.trim();
            String pass = params.substring(passIdx + 5);
            pass.trim();

            if (ssid.length() > 0) {
                saveWiFiConfig(ssid, pass);
                WiFi.disconnect();
                connectWiFi();
            } else {
                Serial.println("Error: Invalid SSID");
            }
        } else {
            Serial.println("Error: Usage: wifi ssid=<ssid> pass=<password>");
        }

    } else if (command.startsWith("mqtt ")) {
        // Parse: mqtt broker=192.168.1.100 port=1883 user=admin pass=secret
        String params = command.substring(5);
        String broker = "";
        uint16_t port = 1883;
        String user = "";
        String pass = "";

        int brokerIdx = params.indexOf("broker=");
        if (brokerIdx >= 0) {
            int nextSpace = params.indexOf(' ', brokerIdx);
            if (nextSpace < 0) nextSpace = params.length();
            broker = params.substring(brokerIdx + 7, nextSpace);
            broker.trim();
        }

        int portIdx = params.indexOf("port=");
        if (portIdx >= 0) {
            int nextSpace = params.indexOf(' ', portIdx);
            if (nextSpace < 0) nextSpace = params.length();
            String portStr = params.substring(portIdx + 5, nextSpace);
            port = portStr.toInt();
        }

        int userIdx = params.indexOf("user=");
        if (userIdx >= 0) {
            int nextSpace = params.indexOf(' ', userIdx);
            if (nextSpace < 0) nextSpace = params.length();
            user = params.substring(userIdx + 5, nextSpace);
            user.trim();
        }

        int passIdx = params.indexOf("pass=");
        if (passIdx >= 0) {
            pass = params.substring(passIdx + 5);
            pass.trim();
        }

        if (broker.length() > 0) {
            saveMQTTConfig(broker, port, user, pass);
            mqttClient.disconnect();
            connectMQTT();
        } else {
            Serial.println("Error: Usage: mqtt broker=<host> port=<port> [user=<user> pass=<pass>]");
        }

    } else {
        Serial.println("Unknown command. Type 'help' for usage.");
    }
}

void handleUartData() {
    String line = UartS3.readStringUntil('\n');
    line.trim();

    if (line.length() == 0) return;

    Serial.println("[UART] " + line);

    // Parse JSON command from ESP32-S3
    JsonDocument doc;
    DeserializationError error = deserializeJson(doc, line);

    if (error) {
        Serial.println("[UART] JSON parse error: " + String(error.c_str()));
        return;
    }

    String cmd = doc["cmd"].as<String>();

    if (cmd == "pub") {
        // Publish grind session to MQTT
        if (mqttConnected && doc["data"].is<JsonObject>()) {
            publishSession(doc["data"]);
        } else {
            Serial.println("[MQTT] Not connected or invalid data, cannot publish");
        }
    } else if (cmd == "status") {
        // Send status back to ESP32-S3
        sendStatus();
    }
}

void publishSession(const JsonDocument& session) {
    uint32_t sessionId = session["session_id"] | 0;
    String topic = "grinder/" + deviceID + "/sessions/" + String(sessionId);

    String payload;
    serializeJson(session, payload);

    bool success = mqttClient.publish(topic.c_str(), payload.c_str(), true);

    if (success) {
        Serial.println("[MQTT] Published session " + String(sessionId) + " (" + String(payload.length()) + " bytes)");
    } else {
        Serial.println("[MQTT] Failed to publish session " + String(sessionId));
    }
}

void sendStatus() {
    JsonDocument doc;
    doc["status"] = "ok";
    doc["wifi"] = wifiConnected;
    doc["mqtt"] = mqttConnected;
    if (wifiConnected) {
        doc["ip"] = WiFi.localIP().toString();
    }

    String json;
    serializeJson(doc, json);
    UartS3.println(json);

    Serial.println("[UART] Sent status: " + json);
}

void printStatus() {
    Serial.println("\n=== Gateway Status ===");
    Serial.println("Device ID: " + deviceID);
    Serial.println("WiFi SSID: " + (wifiSSID.length() > 0 ? wifiSSID : "(not configured)"));
    Serial.println("WiFi Status: " + String(wifiConnected ? "Connected (" + WiFi.localIP().toString() + ")" : "Disconnected"));
    Serial.println("MQTT Broker: " + (mqttBroker.length() > 0 ? mqttBroker + ":" + String(mqttPort) : "(not configured)"));
    Serial.println("MQTT Status: " + String(mqttConnected ? "Connected" : "Disconnected"));
    Serial.println("======================\n");
}
