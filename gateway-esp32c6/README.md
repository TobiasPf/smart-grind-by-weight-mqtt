# ESP32-C3 SuperMini WiFi/MQTT Gateway

Standalone firmware for ESP32-C3 SuperMini that receives grind session data from the main ESP32-S3 coffee scale via UART and publishes to an MQTT broker.

## Purpose

This gateway offloads WiFi/MQTT functionality to a separate ESP32-C3, completely avoiding SPI bus conflicts with the ESP32-S3's display.

## Hardware Setup

### Wiring Between Boards

```
ESP32-S3 (Coffee Scale)    ESP32-C3 SuperMini (Gateway)
-----------------------    ----------------------------
TX (GPIO43)            →   RX (GPIO20)
RX (GPIO44)            ←   TX (GPIO21)
GND                    —   GND
```

### Power

- ESP32-S3: Powered via USB or battery
- ESP32-C3: Powered via separate USB-C connection

## Building and Flashing

```bash
cd gateway-esp32c6
platformio run --target upload --upload-port /dev/ttyACM0  # or COM port on Windows
```

## Configuration via Serial

Connect to the ESP32-C3 via USB serial at 115200 baud.

### Configure WiFi (ESP32-C3)

```
wifi ssid=YourNetwork pass=YourPassword
```

### Configure MQTT

```
mqtt broker=192.168.1.100 port=1883 user=admin pass=secret
```

Or without authentication:
```
mqtt broker=192.168.1.100 port=1883
```

### Check Status

```
status
```

Output:
```
=== Gateway Status ===
Device ID: A1B2C3D4E5F6
WiFi SSID: YourNetwork
WiFi Status: Connected (192.168.1.50)
MQTT Broker: 192.168.1.100:1883
MQTT Status: Connected
======================
```

### Reset Configuration

```
reset
```

Clears all settings and restarts.

## UART Protocol

### ESP32-S3 → ESP32-C3

**Publish Session:**
```json
{"cmd":"pub","data":{
  "device_id":"A1B2C3",
  "session_id":123,
  "timestamp":1234567890,
  "mode":"weight",
  "target_weight":18.0,
  "final_weight":18.2,
  ...
}}
```

**Request Status:**
```json
{"cmd":"status"}
```

### ESP32-C3 → ESP32-S3

**Status Response:**
```json
{"status":"ok","wifi":true,"mqtt":true,"ip":"192.168.1.50"}
```

## MQTT Topics

Sessions are published to:
```
grinder/{device_id}/sessions/{session_id}
```

With retained flag set for persistence.

Gateway status:
```
grinder/{device_id}/status
```

## LED Indicators

- Built-in LED blinks during UART communication

## Troubleshooting

### WiFi won't connect
- Check SSID and password spelling
- Verify 2.4GHz network (ESP32-C3 doesn't support 5GHz)
- Check signal strength

### MQTT won't connect
- Verify broker IP/hostname
- Check firewall settings
- Test broker with `mosquitto_sub` tool
- Verify credentials if using authentication

### No data from ESP32-S3
- Check UART wiring (TX→RX, RX→TX, GND→GND)
- Verify baud rate (115200)
- Check serial monitor for UART messages

## Serial Monitor Commands

Type `help` to see all available commands:

```
Available commands:
  wifi ssid=<ssid> pass=<password>
  mqtt broker=<host> port=<port> [user=<user> pass=<pass>]
  status
  reset
  help
```
