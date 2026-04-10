# FLEX-FSK-TX v2.5.4

**Dual-Mode UART/Serial Firmware: AT Commands + Binary Protocol with Time Synchronization**

ESP32-based FLEX pager message transmitter with automatic clock synchronization and async event notifications.

---

## Features

### Dual-Mode Interface
- **AT Command Protocol** - Human-readable serial commands
- **Binary Protocol** - High-performance packet-based communication
- **Automatic Detection** - Seamless switching between modes
- **Time Synchronization** - Automatic clock sync with drift correction (v2.5.4)

### Core Functionality
- **FLEX Encoding** - On-device message encoding (up to 248 characters)
- **Message Queue** - 10-message FIFO queue with async transmission
- **EMR Support** - Emergency Message Resynchronization
- **Core 0 Isolation** - Dedicated RF transmission core
- **Event Notifications** - Async events (TX_START, TX_DONE, TX_FAILED)

### Binary Protocol (v2.5.4)
- **Fixed 512-byte packets** with COBS framing
- **CRC16-CCITT** error detection (99.998% error coverage)
- **UUID tracking** for message correlation (128-bit)
- **Timestamp header** (8 bytes): Unix timestamp, milliseconds, timezone
- **Auto clock sync**: Drift > 1 second triggers correction
- **Latency measurement**: Client ↔ ESP32 round-trip timing

### Configuration & Storage
- **NVS (Preferences)** - Persistent core config (PPM correction)
- **SPIFFS** - Application settings in JSON format
- **Persistent Logging** - 250KB max with automatic rotation
- **Factory Reset** - GPIO 0 button (30s hold) or AT command

### Hardware Support
- **Devices**: TTGO LoRa32-OLED, Heltec WiFi LoRa 32 V2
- **Radio**: SX1276 (433/868/915 MHz)
- **RF Amplifier Control** - Configurable GPIO, polarity, delay
- **Battery Monitoring** - Voltage, percentage, alerts
- **RTC Support (DS3231)** - Hardware clock synchronization
- **OLED Display** - Status display with 5-minute timeout
- **Heartbeat LED** - 4 blinks every 60 seconds

### Diagnostics & Reliability
- **Watchdog Timer** - 60-second timeout with boot protection
- **Boot Failure Tracking** - Auto-recovery after failures
- **Message Truncation** - Auto-truncate to 248 chars
- **Capcode Validation** - FLEX protocol range validation

---

## AT Commands Reference

### Basic Commands
```
AT                          - Test AT interface
AT+RESET                    - Reset device
AT+ABORT                    - Abort current operation
AT+STATUS?                  - Query device state
AT+DEVICE?                  - Comprehensive device info
```

### Radio Configuration
```
AT+FREQ=<MHz>               - Set frequency (400-1000 MHz)
AT+FREQ?                    - Query frequency
AT+FREQPPM=<ppm>            - Set PPM correction (-50 to +50)
AT+FREQPPM?                 - Query PPM correction
AT+POWER=<dBm>              - Set TX power (2-20 dBm)
AT+POWER?                   - Query TX power
```

### Message Transmission
```
AT+SEND=<bytes>             - Send binary data
AT+MSG=<capcode>            - Send FLEX message (text follows)
AT+MAILDROP=<0|1>           - Set/query mail drop flag
AT+MAILDROP?
```

### Clock Configuration (v2.5.4)
```
AT+CCLK=<timestamp>,<tz>    - Set clock manually
AT+CCLK?                    - Query current time

Examples:
  AT+CCLK=1775426200,-6.0   # Set to 2026-04-05 15:30:00 UTC-6
  AT+CCLK?
  Response: +CCLK: 1775426200,-6.0,2026-04-05 15:30:00
            └─ UTC timestamp └─ Timezone └─ Local time
  OK
```

### FLEX Defaults
```
AT+FLEX?                    - Query all FLEX defaults
AT+FLEX=CAPCODE,<capcode>   - Set default capcode
AT+FLEX=FREQUENCY,<MHz>     - Set default frequency
AT+FLEX=POWER,<dBm>         - Set default power
```

### Logging
```
AT+LOGS?<N>                 - Query last N log lines (default 25)
AT+RMLOG                    - Delete log file
```

### Factory Reset
```
AT+FACTORYRESET             - Factory reset (clear SPIFFS)
```

---

## Binary Protocol Specification

### Packet Structure (512 bytes fixed)

```
Offset    | Size | Field              | Description
----------|------|--------------------|-----------------------------------------
[0]       | 1    | type               | Packet type (CMD/RSP/EVT)
[1]       | 1    | opcode             | Operation code
[2]       | 1    | flags              | Control flags
[3]       | 1    | seq                | Sequence number (0-255, wraps)
[4-19]    | 16   | uuid               | 128-bit UUID (RFC 4122 v4)
[20-21]   | 2    | payload_len        | Valid bytes in payload (big-endian)
[22-501]  | 480  | payload            | Variable payload data
[502-509] | 8    | timestamp          | Timestamp header (v2.5.3+)
[510-511] | 2    | crc16              | CRC16-CCITT checksum
```

**Design Rationale:**
- Compact header (22 bytes) for efficient parsing
- Maximum payload (480 bytes) for message data
- Timestamp positioned before CRC for integrity protection
- CRC at fixed offset 510 validates entire packet


### Timestamp Header (8 bytes) - v2.5.4

```c
typedef struct __attribute__((packed)) {
    uint32_t unix_timestamp;    // [0-3]  UTC seconds since 1970 (big-endian)
    uint16_t milliseconds;      // [4-5]  Subseconds 0-999 (big-endian)
    int8_t   timezone_offset;   // [6]    Timezone in 30-min units (-48 to +56)
    uint8_t  flags;             // [7]    Control flags
} timestamp_header_t;
```

**Timezone Examples:**
- `UTC-6`:   `-12` (calculated as: 6 × 2)
- `UTC+0`:   `0`
- `UTC+5:30`: `11` (calculated as: 5.5 × 2)

**Timestamp Flags:**
```c
#define TS_FLAG_VALID       0x01  // Timestamp is valid
#define TS_FLAG_AUTO_ADJUST 0x02  // Auto-adjust clock if drift > 1s
#define TS_FLAG_SYNC_RTC    0x04  // Sync RTC hardware
#define TS_FLAG_DST_ACTIVE  0x08  // Daylight Saving Time active
```

### Framing: COBS (Consistent Overhead Byte Stuffing)

Binary packets are COBS-encoded before transmission to ensure binary-safe communication:
- **Delimiter**: `0x00` byte marks frame boundaries
- **Overhead**: ~0.4% (1 byte per 254 bytes worst case)
- **Benefit**: Fast frame synchronization, no escaping needed

### Packet Types

| Type | Value | Description |
|------|-------|-------------|
| CMD  | 0x01  | Command (Host → ESP32) |
| RSP  | 0x02  | Response (ESP32 → Host, immediate) |
| EVT  | 0x03  | Event (ESP32 → Host, async) |

### Commands (CMD)

| Opcode | Name               | Payload | Description |
|--------|--------------------|---------|-------------|
| 0x01   | CMD_SEND_FLEX      | capcode(4) + frequency(4) + power(1) + mail(1) + len(1) + message | Send FLEX message |
| 0x02   | CMD_GET_STATUS     | empty | Query device status |
| 0x06   | CMD_PING           | empty | Heartbeat/connectivity test |
| 0x09   | CMD_FACTORY_RESET  | empty | Factory reset device |

### Responses (RSP)

| Opcode | Name       | Payload | Description |
|--------|------------|---------|-------------|
| 0x01   | RSP_ACK    | status(1) | Command accepted |
| 0x02   | RSP_NACK   | status(1) | Command rejected |
| 0x03   | RSP_STATUS | state(1) + queue(1) + battery(3) + freq(4) + power(1) | Status information |
| 0x05   | RSP_PONG   | empty | Ping response |

### Events (EVT)

| Opcode | Name                   | Payload | Description |
|--------|------------------------|---------|-------------|
| 0x01   | EVT_TX_QUEUED          | pos(1) | Message queued |
| 0x02   | EVT_TX_START           | empty | Transmission started |
| 0x03   | EVT_TX_DONE            | result(1) | Transmission completed |
| 0x04   | EVT_TX_FAILED          | error(1) | Transmission failed |
| 0x07   | EVT_BATTERY_LOW        | pct(1) | Low battery alert |

---

## Time Synchronization (v2.5.4)

### Automatic Sync (Binary Protocol)

The client automatically includes system timestamp in every packet:

```
┌─────────────────────────────────────────┐
│ Client (Host with system clock)         │
├─────────────────────────────────────────┤
│ 1. Gets timestamp: gettimeofday()       │
│ 2. Builds CMD packet with timestamp     │
│ 3. Sends → ESP32                        │
└─────────────────────────────────────────┘
              ↓
┌─────────────────────────────────────────┐
│ ESP32 Processing                        │
├─────────────────────────────────────────┤
│ 1. Extracts host timestamp              │
│ 2. Compares with device time            │
│ 3. If drift > 1 sec AND AUTO_ADJUST:    │
│    • Updates system clock                │
│    • Updates timezone                    │
│    • Syncs RTC if available              │
│    • Logs: "CCLK: Auto-adjusted..."     │
└─────────────────────────────────────────┘
              ↓
┌─────────────────────────────────────────┐
│ ESP32 Response                          │
├─────────────────────────────────────────┤
│ • Responds with ESP32's timestamp       │
│ • Includes current timezone             │
│ • Client calculates latency             │
└─────────────────────────────────────────┘
```

### Manual Sync (AT Command)

```bash
# Set clock with Unix timestamp and timezone
AT+CCLK=1775426200,-6.0
OK

# Query current time
AT+CCLK?
+CCLK: 1775426200,-6.0,2026-04-05 15:30:00
OK
```

---

## Quick Start

### Hardware Requirements
- ESP32 LoRa board (TTGO LoRa32-OLED or Heltec WiFi LoRa 32 V2)
- SX1276 radio module
- USB cable for programming/serial

### Compilation

#### Using flex-build-upload.sh script:
```bash
flex-build-upload.sh -t heltec -u flex-fsk-tx-v2.5.ino
```

#### Using arduino-cli directly:
```bash
# Heltec WiFi LoRa 32 V2
arduino-cli compile --fqbn esp32:esp32:heltec_wifi_lora_32_V2 \
  --build-property compiler.cpp.extra_flags=-DHELTEC_WIFI_LORA32_V2 \
  flex-fsk-tx-v2.5.ino

arduino-cli upload -p /dev/ttyUSB0 --fqbn esp32:esp32:heltec_wifi_lora_32_V2

# TTGO LoRa32-OLED
arduino-cli compile --fqbn esp32:esp32:ttgo-lora32 flex-fsk-tx-v2.5.ino
arduino-cli upload -p /dev/ttyACM0 --fqbn esp32:esp32:ttgo-lora32
```

### Client Compilation

```bash
cd tools
gcc -o flex-binary-client flex-binary-client.c -O2 -Wall
```

### Required Arduino Libraries

Install via Arduino Library Manager:
- **RadioLib** by Jan Gromeš (all versions)
- **U8g2** by oliver (TTGO only, Heltec uses built-in)
- **ArduinoJson** by Benoit Blanchon
- **RTClib** by Adafruit (optional, if RTC_ENABLED = true)
- **Heltec ESP32 Dev-Boards** by Heltec Automation (Heltec only)

---

## Usage Examples

### AT Command Mode

```bash
# Connect via serial terminal (115200 baud)
screen /dev/ttyUSB0 115200

# Configure radio
AT+FREQ=931.9375
OK
AT+POWER=10
OK

# Send FLEX message
AT+MSG=1234567
Hello from ESP32!
OK

# Query device status
AT+STATUS?
+STATUS: IDLE,0,85%,4150mV,931.9375MHz,10dBm
OK
```

### Binary Protocol Mode

```bash
# Send message with verbose output (shows timestamps and latency)
tools/flex-binary-client -d /dev/ttyUSB0 -f 931.9375 -p 10 \
  1234567 "Hello Binary Protocol" -v

# Output:
Device: /dev/ttyUSB0
Baudrate: 115200
Frequency: 931.9375 MHz
Power: 10 dBm
Mail drop: false
Capcode: 1234567
Message: Hello Binary Protocol
Length: 22 bytes
Client Time: 2026-04-05 15:30:45.123 (UTC-6.0)

Connected to /dev/ttyUSB0 @ 115200 baud
Sending message (uuid=..., capcode=1234567, ...)
ESP32 Time: 2026-04-05 15:30:45.456 (UTC-6.0) | Latency: ~333 ms
ACK: Message accepted
Done.
```

### Client Command Options

```
Usage: flex-binary-client [OPTIONS] CAPCODE MESSAGE

Options:
  -d DEVICE    Serial device (default: /dev/ttyUSB0)
  -b BAUD      Baudrate (default: 115200)
  -f FREQ      Frequency in MHz (default: 931.9375)
  -p POWER     Power in dBm (default: 10)
  -m           Enable mail drop flag
  -v           Verbose output (shows timestamps and latency)
  -w           Wait for TX_DONE event
  -h           Show help

Examples:
  flex-binary-client 1234567 "Hello World"
  flex-binary-client -d /dev/ttyUSB0 -f 931.9375 -p 15 -w 1234567 "Test"
  flex-binary-client -v -m 37137 "Mail drop message"
```

---

## Architecture

### Dual-Core Design

```
┌────────────────────────────────────────────────────────────┐
│                      ESP32 Dual-Core                        │
├──────────────────────────────┬─────────────────────────────┤
│       CORE 1 (240 MHz)       │      CORE 0 (240 MHz)       │
│                              │                             │
│  • AT command parser         │  • Transmission task        │
│  • Binary protocol handler   │  • FLEX encoding            │
│  • Serial I/O                │  • Radio TX (SX1276)        │
│  • Drift check & clock sync  │  • EMR handling             │
│  • Message queuing           │  • Event notifications      │
│  • Display updates           │                             │
│  • Battery monitoring        │  Priority: 1 (high)         │
└──────────────────────────────┴─────────────────────────────┘
         │                                    │
         ▼                                    ▼
    UART Serial                          SX1276 Radio
    115200 baud                          FSK @ 1.6kbps
```

### Message Flow (Binary Protocol)

```
1. Client → ESP32 (CMD_SEND_FLEX with timestamp)
   ├─ COBS framing
   ├─ CRC16 validation
   ├─ Drift check & clock sync
   └─ Queue message

2. ESP32 → Client (RSP_ACK with ESP32 timestamp)
   └─ Message accepted/rejected

3. ESP32 → Client (EVT_TX_START)
   └─ Transmission starting

4. CORE 0: Transmission Task
   ├─ Dequeue message
   ├─ FLEX encoding
   ├─ EMR (if needed)
   └─ Radio transmission

5. ESP32 → Client (EVT_TX_DONE with ESP32 timestamp)
   └─ Transmission complete (latency measurable)
```

---

## File Structure

```
flex-fsk-tx-v2.5/
├── flex-fsk-tx-v2.5.ino    # Main firmware (setup/loop)
├── config.h                 # Configuration constants
│
├── at_commands.h / .cpp     # AT parser + binary handler + drift check
├── binary_packet.h / .cpp   # Packet structures and builders
├── binary_handlers.h / .cpp # Command handlers
├── binary_events.h / .cpp   # Event senders
│
├── cobs.h / .cpp            # COBS framing
├── crc16.h / .cpp           # CRC16-CCITT
├── uuid.h / .cpp            # UUID generation
│
├── flex_protocol.h / .cpp   # FLEX encoding, EMR, queue
├── transmission.h / .cpp    # Core 0 transmission task
│
├── hardware.h / .cpp        # Hardware abstraction (radio, RTC, battery)
├── display.h / .cpp         # OLED display logic
├── storage.h / .cpp         # NVS + SPIFFS management
├── logging.h / .cpp         # Persistent logging system
├── utils.h / .cpp           # Utility functions
│
├── boards/                  # Symlink to ../../include/boards
├── tinyflex/                # Symlink to ../../include/tinyflex
│
├── tools/
│   └── flex-binary-client.c # C client for binary protocol
│
└── README.md                # This file
```

---

## Performance

| Operation | Time | Notes |
|-----------|------|-------|
| COBS Encoding | ~10 µs | 100 bytes @ 240 MHz |
| CRC16 Calculation | ~5 µs | 100 bytes @ 240 MHz |
| Packet Building | ~2 µs | Stack-based |
| Drift Check | ~1 µs | Timestamp comparison |
| FLEX Encoding | ~50 ms | Message-dependent |
| Radio TX | 1-3 sec | Message-dependent |
| Latency (Serial) | 200-1000 ms | Includes TX time |

## Memory Usage (v2.5.4)

| Component | ROM | RAM |
|-----------|-----|-----|
| Core Firmware | ~1.05 MB | ~56 KB |
| Binary Protocol | ~15 KB | ~512 bytes |
| COBS/CRC Tables | ~512 bytes | 0 |
| Message Queue | 0 | ~2.7 KB (10 messages) |
| Total Usage | ~1.08 MB | ~58 KB |

**Available:**
- Flash: 3.3 MB total (68% free)
- RAM: 327 KB total (82% free)

---

## Troubleshooting

### Device Not Responding
- Check serial port: `ls /dev/tty*`
- Verify baudrate: 115200
- Try different USB cable
- Check device power

### Time Sync Not Working
- Ensure firmware is v2.5.4
- Ensure client is v2.5.3+
- Check logs: `AT+LOGS?50`
- Verify drift check logs: "CCLK: Auto-adjusted..."

### Message Not Transmitting
- Check frequency: 400-1000 MHz valid range
- Check power: 0-20 dBm valid range
- Check message length: ≤248 characters
- Check queue status: `AT+STATUS?`

### Binary Protocol CRC Errors
- Check serial cable quality
- Reduce baudrate: 57600
- Check for EMI/noise sources
- Verify COBS framing integrity

---

## Version History

### v2.5.4 (2026-04-05) - Current
- Added AT+CCLK command for manual clock setting
- Timezone sync from binary protocol packets
- Fixed segfault in client with invalid timestamps
- Latency measurement in verbose mode
- Automatic clock drift correction

### v2.5.3 (2026-04-05)
- Added timestamp header (8 bytes) in binary packets
- Reduced payload from 486 to 478 bytes
- Client auto-includes system timestamp
- ESP32 responds with its timestamp (latency measurement)
- Auto clock drift adjustment (> 1 sec) with RTC sync
- CRC remains at bytes 510-511 (unchanged)

### v2.5.2 (2026-04-05)
- Enabled UUID for msg_id consistency
- Binary packet fixed size to 512 bytes
- Code cleanup: removed inline comments

### v2.5.1 (2026-04-04)
- Initial binary protocol implementation
- COBS framing + CRC16-CCITT
- Dual-mode AT + Binary detection
- Message ID correlation with UUID
- Async events (TX_QUEUED, TX_START, TX_DONE, TX_FAILED)
- 100% backward compatible with AT command mode

### v2.5.0 (2026-04-03)
- Initial modular release
- Combined v2 and v3.6 features without WiFi dependencies
- AT command protocol only

---

## Documentation

### Protocol Specifications
- **[docs/BINARY_PROTOCOL.md](docs/BINARY_PROTOCOL.md)** — Complete binary protocol specification
  - Packet structure and field layouts
  - All opcodes, commands, responses, and events
  - Payload structures for each command type
  - UUID/SEQ semantics and message correlation
  - CRC16-CCITT specification
  - Communication patterns and error handling

- **[docs/COBS_ENCAPSULATION.md](docs/COBS_ENCAPSULATION.md)** — COBS framing details
  - Algorithm explanation with examples
  - C and Python implementations
  - Test vectors and wire format
  - Frame synchronization rules

- **[docs/AT_COMMANDS.md](docs/AT_COMMANDS.md)** — AT command reference
  - Complete command list with syntax
  - Configuration commands (radio, WiFi, clock)
  - Status queries and logging
  - Troubleshooting guide

### Client Libraries
- **[tools/README.md](tools/README.md)** — Binary protocol tools overview
- **[tools/libflex_binary/README.md](tools/libflex_binary/README.md)** — C library architecture
  - Internal design and data structures
  - Packet building, COBS, CRC, UUID generation
  - Serial port management and frame reception
  - Complete API documentation

- **[tools/python/README.md](tools/python/README.md)** — Python library architecture
  - Internal design and exception hierarchy
  - Packet building and parsing internals
  - Usage examples and performance notes
  - Debugging and troubleshooting

---

## License

GNU General Public License v3.0

---

## References

- FLEX Protocol: Motorola FLEX™ Paging Protocol
- COBS: Stuart Cheshire and Mary Baker, "Consistent Overhead Byte Stuffing"
- CRC16-CCITT: ITU-T Recommendation V.41 (Polynomial 0x1021)
- RadioLib: https://github.com/jgromes/RadioLib
- tinyflex: Single-header FLEX protocol implementation
