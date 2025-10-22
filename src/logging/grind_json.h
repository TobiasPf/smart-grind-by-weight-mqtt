#pragma once

#include <Arduino.h>
#include <ArduinoJson.h>
#include "grind_logging.h"
#include "../config/constants.h"

/**
 * GrindSessionSerializer - Converts GrindSession structs to JSON strings
 *
 * Serializes grind session metadata to JSON format for MQTT publishing.
 * Includes device identification and all relevant session metrics.
 */
class GrindSessionSerializer {
public:
    /**
     * Get unique device ID based on ESP32 chip ID
     * @return Device ID string (e.g., "esp32-a1b2c3d4")
     */
    static String get_device_id();

    /**
     * Serialize a GrindSession to JSON string
     * @param session Pointer to GrindSession struct to serialize
     * @param out Output String to write JSON to
     * @return true if serialization succeeded, false on error
     */
    static bool serialize_session_to_json(const GrindSession* session, String& out);

    /**
     * Get termination reason as human-readable string
     * @param reason GrindTerminationReason enum value
     * @return String representation of termination reason
     */
    static const char* termination_reason_to_string(GrindTerminationReason reason);

    /**
     * Get grind mode as human-readable string
     * @param mode GrindMode enum value
     * @return String representation of grind mode
     */
    static const char* grind_mode_to_string(uint8_t mode);
};
