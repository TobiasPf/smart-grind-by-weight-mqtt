# WiFi/MQTT Network Integration Guide

This guide explains how to configure and use the WiFi/MQTT functionality to upload grind session data to a cloud MQTT broker.

## Overview

The ESP32-S3 coffee grinder scale can connect to WiFi and publish grind session summaries to an MQTT broker in JSON format. This enables:
- Cloud logging of all grinding sessions
- Real-time monitoring of grinder usage
- Integration with home automation systems (Home Assistant, Node-RED, etc.)
- Historical data analysis and visualization

## Architecture

**Network Layer** (`src/network/`):
- **WiFiManager**: Manages WiFi connection with automatic reconnection
- **MQTTManager**: Handles MQTT broker connection and session publishing
- **NetworkConfigService**: BLE service for provisioning credentials

**FreeRTOS Integration**:
- **NetworkTask**: Dedicated task on Core 1 running at 500ms intervals
- **network_publish_queue**: Queue for grind sessions awaiting upload

**Data Flow**:
1. Grind completes → Session saved to LittleFS
2. Session copied to `network_publish_queue`
3. NetworkTask dequeues session → JSON serialization
4. MQTTManager publishes to `grinder/{device_id}/sessions/{session_id}`

## Configuration

### Prerequisites

1. **WiFi Network**: SSID and password for your 2.4GHz network (ESP32 does not support 5GHz)
2. **MQTT Broker**: Public or private MQTT broker (e.g., Mosquitto, HiveMQ, CloudMQTT)
   - Broker address (hostname or IP)
   - Port (default: 1883)
   - Optional: username and password for authentication

### Setup via BLE (Recommended)

Use the Python CLI tool to configure WiFi and MQTT settings over Bluetooth:

```bash
# 1. Configure WiFi credentials
python3 tools/grinder.py wifi-config --ssid "YourNetworkName" --password "YourPassword"

# 2. Configure MQTT broker
python3 tools/grinder.py mqtt-config --broker "mqtt.example.com" --port 1883 --user "grinder" --pass "secret"

# 3. Enable WiFi
python3 tools/grinder.py network-enable

# 4. Check connection status
python3 tools/grinder.py network-status
```

**Note**: The Python CLI commands for network provisioning will be implemented in the future. For now, use the BLE characteristics directly.

### Setup via BLE Characteristics (Advanced)

If you're using a custom BLE client, you can write directly to the Network Config Service:

**Service UUID**: `88990011-aabb-ccdd-eeff-112233445566`

**Characteristics**:

1. **WiFi Credentials** (UUID: `99001122-bbcc-ddee-ff11-223344556677`)
   - **Write**: `"SSID|password"`
   - Example: `"MyNetwork|SecurePass123"`

2. **MQTT Config** (UUID: `00112233-ccdd-eeff-1122-334455667788`)
   - **Write**: `"host:port|username|password"`
   - Example: `"mqtt.example.com:1883|user|pass"`
   - For no authentication: `"mqtt.example.com:1883||"`

3. **Network Control** (UUID: `22334455-eeff-1122-3344-556677889900`)
   - **Write**: Control commands (single byte)
     - `0x01`: Enable WiFi
     - `0x02`: Disable WiFi
     - `0x03`: Enable MQTT
     - `0x04`: Disable MQTT
     - `0x05`: Test connection
     - `0x06`: Get status

4. **Network Status** (UUID: `11223344-ddee-ff11-2233-445566778899`)
   - **Read/Notify**: JSON status object
   ```json
   {
     "wifi": {
       "enabled": true,
       "connected": true,
       "status": "connected",
       "ssid": "MyNetwork",
       "ip": "192.168.1.100",
       "rssi": -45
     },
     "mqtt": {
       "enabled": true,
       "connected": true,
       "status": "connected",
       "broker": "mqtt.example.com",
       "port": 1883,
       "pending_publishes": 0
     }
   }
   ```

## Published Data Format

Each grind session is published as a JSON message to:
```
grinder/{device_id}/sessions/{session_id}
```

**Example Device ID**: `esp32-a1b2c3d4` (derived from ESP32 chip ID)

**Example Topic**: `grinder/esp32-a1b2c3d4/sessions/42`

**JSON Payload** (Weight Mode):
```json
{
  "device_id": "esp32-a1b2c3d4",
  "session_id": 42,
  "timestamp": 1729584000,
  "duration_ms": 15420,
  "motor_on_time_ms": 12350,
  "mode": "weight",
  "profile_id": 1,
  "target_weight": "18.0",
  "final_weight": "18.2",
  "error_grams": "0.20",
  "tolerance": "0.5",
  "pulse_count": 3,
  "max_pulse_attempts": 10,
  "termination_reason": "completed",
  "result_status": "Target reached",
  "controller": {
    "motor_stop_offset": "0.80",
    "latency_coast_ratio": "1.250",
    "flow_rate_threshold": "2.50"
  }
}
```

**JSON Payload** (Time Mode):
```json
{
  "device_id": "esp32-a1b2c3d4",
  "session_id": 43,
  "timestamp": 1729584100,
  "duration_ms": 10000,
  "motor_on_time_ms": 10000,
  "mode": "time",
  "profile_id": 0,
  "target_time_ms": 10000,
  "time_error_ms": 0,
  "final_weight": "17.5",
  "start_weight": "0.0",
  "pulse_count": 0,
  "max_pulse_attempts": 10,
  "termination_reason": "completed",
  "result_status": "Time complete",
  "controller": {
    "motor_stop_offset": "0.80",
    "latency_coast_ratio": "1.250",
    "flow_rate_threshold": "2.50"
  }
}
```

**Termination Reasons**:
- `completed`: Target reached successfully
- `timeout`: 30-second timeout exceeded
- `overshoot`: Weight exceeded target + tolerance
- `max_pulses`: Maximum pulse attempts reached (10)
- `unknown`: Unexpected termination

## MQTT Integration Examples

### Mosquitto (Local Broker)

```bash
# Install Mosquitto
brew install mosquitto  # macOS
sudo apt-get install mosquitto mosquitto-clients  # Linux

# Start broker
mosquitto -v

# Subscribe to all grind sessions
mosquitto_sub -h localhost -t "grinder/+/sessions/#" -v
```

### Home Assistant

Add to `configuration.yaml`:

```yaml
mqtt:
  sensor:
    - name: "Coffee Grinder Last Session"
      state_topic: "grinder/esp32-a1b2c3d4/sessions/+"
      value_template: "{{ value_json.final_weight }}"
      unit_of_measurement: "g"
      json_attributes_topic: "grinder/esp32-a1b2c3d4/sessions/+"
      json_attributes_template: "{{ value_json | tojson }}"

    - name: "Coffee Grinder Mode"
      state_topic: "grinder/esp32-a1b2c3d4/sessions/+"
      value_template: "{{ value_json.mode }}"
```

### Node-RED

Create a flow:
1. **MQTT In** node: Subscribe to `grinder/+/sessions/#`
2. **JSON** node: Parse payload
3. **Function** node: Process data
4. **InfluxDB Out** / **Dashboard** nodes: Store and visualize

## Troubleshooting

### WiFi Won't Connect

**Check**:
1. SSID and password are correct (case-sensitive)
2. Network is 2.4GHz (ESP32 doesn't support 5GHz)
3. Network security is WPA/WPA2 (WPA3 not supported)
4. Check serial logs: `[WiFi] Status: CONNECTING/CONNECTED/ERROR`

**Reconnection**:
- WiFiManager attempts reconnection with exponential backoff (2s, 4s, 8s, 16s, 30s max)
- After 3 failed attempts, status changes to ERROR

### MQTT Won't Connect

**Check**:
1. WiFi is connected first
2. Broker address and port are correct
3. Broker is reachable from your network
4. Username/password are correct if authentication is enabled
5. Check serial logs: `[MQTT] Status: CONNECTING/CONNECTED/ERROR`

**Test Connection**:
```bash
# From command line
mosquitto_pub -h your-broker.com -p 1883 -u username -P password -t "test" -m "hello"
```

### Sessions Not Publishing

**Check**:
1. WiFi and MQTT are both connected
2. Network task is running (check serial logs)
3. Queue not full (max 10 pending publishes)
4. Check serial logs: `[MQTT] Publishing session X to grinder/...`

**Queue Status**:
- Read Network Status characteristic to see `pending_publishes` count
- If queue is full, oldest sessions may be dropped

### Memory Issues

**If you experience crashes or freezes**:
1. NetworkTask stack size: 6KB (adjustable in `src/config/network.h`)
2. MQTT payload buffer: Default 256 bytes (may need increase for large JSON)
3. Network publish queue: Max 10 sessions (adjustable in `src/config/network.h`)

## Configuration Storage

All network settings are stored in NVS (Non-Volatile Storage):

**Namespace**: `"network"`

**Keys**:
- `wifi_ssid` (String)
- `wifi_password` (String)
- `wifi_enabled` (Bool)
- `mqtt_broker` (String)
- `mqtt_port` (UInt16)
- `mqtt_username` (String)
- `mqtt_password` (String)
- `mqtt_enabled` (Bool)

**Clearing Settings**:
Settings persist across reboots and firmware updates. To clear:
```bash
# Via BLE (future implementation)
python3 tools/grinder.py network-clear

# Via serial console
# Press RESET while holding BOOT button, then re-flash firmware
```

## Security Considerations

1. **WiFi Credentials**: Stored in plain text in NVS (future: encryption support)
2. **MQTT Credentials**: Stored in plain text in NVS
3. **SSL/TLS**: Not currently supported (future: add PubSubClient SSL support)
4. **BLE Provisioning**: Unencrypted (recommend BLE pairing for security)

**Best Practices**:
- Use a dedicated MQTT user account with limited permissions
- Consider using a local MQTT broker instead of cloud services
- Use strong, unique passwords
- Disable BLE when not needed (reduces attack surface)

## Performance Impact

**Resource Usage**:
- **RAM**: ~6KB for NetworkTask stack + MQTT buffers
- **CPU**: Core 1, Priority 2, 500ms interval (minimal impact)
- **Network**: ~500 bytes per session publish (depending on JSON size)
- **Flash**: Settings stored in NVS (~200 bytes)

**Battery Impact** (if using battery power):
- WiFi adds ~100mA current draw when active
- MQTT keepalive every 60 seconds
- Consider disabling WiFi when on battery

## Future Enhancements

Planned features:
- [ ] SSL/TLS support for secure MQTT connections
- [ ] Encrypted credential storage
- [ ] WiFi AP mode for initial setup
- [ ] OTA firmware updates via MQTT
- [ ] Configurable publish intervals (real-time vs. batch)
- [ ] Offline queueing (persist sessions to flash when disconnected)
- [ ] Python CLI commands for network provisioning
- [ ] UI screens for network status and settings

## Support

For issues, questions, or feature requests related to WiFi/MQTT functionality:
1. Check serial logs for error messages
2. Review this guide for troubleshooting steps
3. Open an issue on GitHub with logs and configuration details
