#include "grind_json.h"
#include <esp_chip_info.h>
#include <esp_mac.h>

String GrindSessionSerializer::get_device_id() {
    uint64_t chip_id = 0;
    esp_efuse_mac_get_default((uint8_t*)(&chip_id));

    char device_id[32];
    snprintf(device_id, sizeof(device_id), NETWORK_DEVICE_ID_FORMAT, chip_id);
    return String(device_id);
}

const char* GrindSessionSerializer::termination_reason_to_string(GrindTerminationReason reason) {
    switch (reason) {
        case GrindTerminationReason::COMPLETED:
            return "completed";
        case GrindTerminationReason::TIMEOUT:
            return "timeout";
        case GrindTerminationReason::OVERSHOOT:
            return "overshoot";
        case GrindTerminationReason::MAX_PULSES:
            return "max_pulses";
        case GrindTerminationReason::UNKNOWN:
        default:
            return "unknown";
    }
}

const char* GrindSessionSerializer::grind_mode_to_string(uint8_t mode) {
    switch (mode) {
        case 0: // GrindMode::WEIGHT
            return "weight";
        case 1: // GrindMode::TIME
            return "time";
        default:
            return "unknown";
    }
}

bool GrindSessionSerializer::serialize_session_to_json(const GrindSession* session, String& out) {
    if (!session) {
        Serial.println("[JSON] Error: Null session pointer");
        return false;
    }

    // Create JSON document with appropriate size
    JsonDocument doc;

    // Device identification
    doc["device_id"] = get_device_id();

    // Session identification
    doc["session_id"] = session->session_id;
    doc["timestamp"] = session->session_timestamp;

    // Timing information
    doc["duration_ms"] = session->total_time_ms;
    doc["motor_on_time_ms"] = session->total_motor_on_time_ms;

    // Grind mode and configuration
    doc["mode"] = grind_mode_to_string(session->grind_mode);
    doc["profile_id"] = session->profile_id;

    // Target and results
    if (session->grind_mode == 0) { // WEIGHT mode
        doc["target_weight"] = serialized(String(session->target_weight, 1));
        doc["final_weight"] = serialized(String(session->final_weight, 1));
        doc["error_grams"] = serialized(String(session->error_grams, 2));
        doc["tolerance"] = serialized(String(session->tolerance, 1));
    } else { // TIME mode
        doc["target_time_ms"] = session->target_time_ms;
        doc["time_error_ms"] = session->time_error_ms;
        // Still include weight for informational purposes
        doc["final_weight"] = serialized(String(session->final_weight, 1));
        doc["start_weight"] = serialized(String(session->start_weight, 1));
    }

    // Pulse information
    doc["pulse_count"] = session->pulse_count;
    doc["max_pulse_attempts"] = session->max_pulse_attempts;

    // Termination and status
    doc["termination_reason"] = termination_reason_to_string(
        static_cast<GrindTerminationReason>(session->termination_reason)
    );
    doc["result_status"] = String(session->result_status);

    // Controller parameters snapshot
    JsonObject controller = doc["controller"].to<JsonObject>();
    controller["motor_stop_offset"] = serialized(String(session->initial_motor_stop_offset, 2));
    controller["latency_coast_ratio"] = serialized(String(session->latency_to_coast_ratio, 3));
    controller["flow_rate_threshold"] = serialized(String(session->flow_rate_threshold, 2));

    // Serialize to string
    if (JSON_SESSION_PRETTY_PRINT) {
        serializeJsonPretty(doc, out);
    } else {
        serializeJson(doc, out);
    }

    // Validate output
    if (out.length() == 0) {
        Serial.println("[JSON] Error: Serialization produced empty output");
        return false;
    }

    return true;
}
