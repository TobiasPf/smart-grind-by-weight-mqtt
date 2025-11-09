# ESP32-C6 Gateway Setup Guide

Complete guide for setting up dual-board WiFi/MQTT using ESP32-C6 SuperMini as a gateway.

## Overview

This solution uses two ESP32 boards:
1. **ESP32-S3** (main coffee scale) - Display, grinding, BLE, UI
2. **ESP32-C6 SuperMini** (gateway) - WiFi, MQTT, network connectivity

They communicate via UART, completely avoiding SPI bus conflicts.

## Step 1: Hardware Setup

### Required Components
- ESP32-S3 Coffee Scale (already set up)
- ESP32-C6 SuperMini
- 3 jumper wires
- 2 USB cables

### Wiring

Connect the boards:

```
ESP32-S3          ESP32-C6 SuperMini
--------          ------------------
GPIO43 (TX)   →   GPIO16 (RX)
GPIO44 (RX)   ←   GPIO17 (TX)
GND           —   GND
```

**Important:** Each board needs its own USB power/connection.

## Step 2: Flash ESP32-C6 Gateway

### From Windows (PowerShell):

```powershell
cd gateway-esp32c6

# Build and flash
..\\tools\\venv\\Scripts\\python.exe -m platformio run --target upload --upload-port COMx
```

Replace `COMx` with your ESP32-C6's COM port.

### From Linux/Mac:

```bash
cd gateway-esp32c6
platformio run --target upload --upload-port /dev/ttyUSB0
```

## Step 3: Configure ESP32-C6 via Serial

Connect to the ESP32-C6 via serial monitor:

```powershell
# Windows
..\\tools\\venv\\Scripts\\python.exe -m platformio device monitor --port COMx --baud 115200

# Linux/Mac
platformio device monitor --port /dev/ttyUSB0 --baud 115200
```

### Configure WiFi

Type:
```
wifi ssid=YourNetworkName pass=YourPassword
```

Example:
```
wifi ssid=HomeWiFi pass=SecretPass123
```

### Configure MQTT

Type:
```
mqtt broker=192.168.1.100 port=1883 user=grinder pass=coffee123
```

Or without authentication:
```
mqtt broker=192.168.1.100 port=1883
```

### Verify Connection

Type:
```
status
```

You should see:
```
=== Gateway Status ===
Device ID: A1B2C3D4E5F6
WiFi SSID: HomeWiFi
WiFi Status: Connected (192.168.1.50)
MQTT Broker: 192.168.1.100:1883
MQTT Status: Connected
======================
```

## Step 4: Update ESP32-S3 Firmware

The ESP32-S3 firmware needs to be updated to use the UART gateway (not yet integrated in this commit).

**Next integration steps:**
1. Add `UARTGateway` instance to `main.cpp`
2. Initialize UART on GPIO43/44 in `setup()`
3. Connect to network task for session publishing
4. Test grind → UART → MQTT flow

## Testing

### Test 1: Gateway Standalone

With ESP32-C6 powered and configured:
1. Check serial monitor shows "WiFi connected"
2. Check serial monitor shows "MQTT connected"
3. Type `status` to verify

### Test 2: UART Communication

Once ESP32-S3 is integrated:
1. Perform a grind on the coffee scale
2. Check ESP32-C6 serial monitor for "[UART] ..." messages
3. Check ESP32-C6 serial monitor for "[MQTT] Published session ..."
4. Verify MQTT message received with `mosquitto_sub`:

```bash
mosquitto_sub -h 192.168.1.100 -t "grinder/#" -v
```

## Troubleshooting

### Gateway won't connect to WiFi
- Verify SSID/password spelling
- Check 2.4GHz network (ESP32-C6 doesn't support 5GHz)
- Type `status` to see error details

### Gateway won't connect to MQTT
- Ping broker from same network
- Check firewall allows port 1883
- Test with `mosquitto_sub` from another machine
- Verify credentials if using authentication

### No UART communication
- Check wiring: TX→RX, RX→TX, GND→GND
- Verify both boards powered
- Check baud rate (115200)
- Monitor both serial ports simultaneously

### MQTT messages not appearing
- Check topic subscription: `grinder/#`
- Verify retained messages: `mosquitto_sub -h broker -t "grinder/#" -v --retained-only`
- Check broker logs

## Serial Commands Reference

| Command | Description | Example |
|---------|-------------|---------|
| `help` | Show all commands | `help` |
| `wifi ssid=<ssid> pass=<pass>` | Configure WiFi | `wifi ssid=MyNet pass=12345678` |
| `mqtt broker=<host> port=<port> [user=<u> pass=<p>]` | Configure MQTT | `mqtt broker=192.168.1.100 port=1883` |
| `status` | Show current status | `status` |
| `reset` | Clear all settings and restart | `reset` |

## MQTT Topic Structure

Sessions published to:
```
grinder/{device_id}/sessions/{session_id}
```

Gateway status:
```
grinder/{device_id}/status
```

Example:
```
grinder/A1B2C3D4E5F6/sessions/42
grinder/A1B2C3D4E5F6/status
```

## Power Considerations

- ESP32-C6 can be powered from ESP32-S3's 3.3V if current is sufficient
- For reliability, use separate USB power for each board
- ESP32-C6 consumes ~80mA with WiFi active

## Next Steps

1. ✅ ESP32-C6 firmware created
2. ✅ UART gateway interface created
3. ⏳ Integrate into ESP32-S3 main firmware
4. ⏳ Connect to file_io_task for publishing
5. ⏳ Test end-to-end grind → MQTT flow

---

**Files Created:**
- `gateway-esp32c6/` - Complete ESP32-C6 gateway firmware
- `src/network/uart_gateway.h/cpp` - UART communication interface
- This setup guide

**Commit:** `aeefdbf` - "Add ESP32-C6 WiFi/MQTT gateway with UART communication"
