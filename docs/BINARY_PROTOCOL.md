# Binary Protocol Specification v2.5.4

## Overview

Binary communication protocol for FLEX transmission system using COBS-encapsulated packets over UART. Supports commands, responses, and asynchronous events with timestamp synchronization and UUID-based message correlation.

## Physical Layer

- **UART**: 115200 baud, 8N1
- **Framing**: COBS (Consistent Overhead Byte Stuffing)
- **Delimiter**: `0x00` byte
- **CRC**: CRC16-CCITT (polynomial 0x1021, init 0xFFFF)

## Packet Structure

### Base Packet Layout (512 bytes total, 514 bytes max on-wire with COBS)

```
Offset    Size  Field           Description
------    ----  -----           -----------
[0]       1     TYPE            Packet type (CMD/RSP/EVT)
[1]       1     OPCODE          Operation code
[2]       1     FLAGS           Bit flags
[3]       1     SEQ             Sequence number (0-255, wraps)
[4-19]    16    UUID            UUID (128-bit, RFC 4122 v4)
[20-21]   2     PAYLOAD_LEN     Payload length in bytes (0-480, big-endian)
[22-501]  480   PAYLOAD         Variable payload data
[502-509] 8     TIMESTAMP       Timestamp header (8-byte struct)
[510-511] 2     CRC16           CRC16-CCITT checksum

Total: 512 bytes raw packet
COBS overhead: +1-2 bytes → 513-514 bytes on-wire
```

### Header Constants

```c
#define PACKET_FIXED_SIZE       512
#define PACKET_HEADER_SIZE      22   // type + opcode + flags + seq + uuid + payload_len
#define PACKET_PAYLOAD_SIZE     480
#define PACKET_TIMESTAMP_SIZE   8
#define PACKET_CRC_OFFSET       510
```

### Packet Types

```c
typedef enum {
    PACKET_TYPE_CMD = 0x01,  // Command (host → device)
    PACKET_TYPE_RSP = 0x02,  // Response (device → host)
    PACKET_TYPE_EVT = 0x03   // Event (device → host, async)
} packet_type_t;
```

### Opcodes

#### Commands (host → device)

```c
#define CMD_PING          0x01  // Ping device
#define CMD_SET_FREQ      0x02  // Set radio frequency
#define CMD_SET_POWER     0x03  // Set transmit power
#define CMD_SEND_RAW      0x04  // Send raw FLEX data
#define CMD_SEND_MSG      0x05  // Send FLEX message (on-device encoding)
#define CMD_GET_STATUS    0x06  // Query device status
#define CMD_SET_BAUD      0x07  // Change UART baud rate
#define CMD_RESET         0x08  // Software reset
#define CMD_GET_VERSION   0x09  // Query firmware version
#define CMD_SET_MAILDROP  0x0A  // Set maildrop flag for next message
#define CMD_SET_TIME      0x0B  // Synchronize device RTC
```

#### Responses (device → host)

```c
#define RSP_ACK           0x81  // Acknowledgment
#define RSP_NAK           0x82  // Negative acknowledgment
#define RSP_STATUS        0x83  // Status response
#define RSP_VERSION       0x84  // Version response
#define RSP_ERROR         0x85  // Error response
```

#### Events (device → host, asynchronous)

```c
#define EVT_TX_START      0xC1  // Transmission started
#define EVT_TX_COMPLETE   0xC2  // Transmission completed
#define EVT_TX_QUEUED     0xC3  // Message queued (not yet transmitted)
#define EVT_ERROR         0xC4  // Error event
#define EVT_READY         0xC5  // Device ready after boot
#define EVT_LOG           0xC6  // Log message
```

### Flags Bitmask

```c
#define FLAG_TIMESTAMP_VALID   0x01  // Timestamp field is valid
#define FLAG_REQUIRES_ACK      0x02  // Packet requires ACK response
#define FLAG_LAST_FRAGMENT     0x04  // Last fragment of multi-packet message
#define FLAG_MORE_FRAGMENTS    0x08  // More fragments follow
#define FLAG_PRIORITY_HIGH     0x10  // High-priority packet
#define FLAG_ENCRYPTED         0x20  // Payload is encrypted (reserved)
```

### Sequence Number (SEQ)

- **Range**: 0-65535 (wraps at overflow)
- **Scope**: Global counter, incremented for every packet sent
- **Purpose**: Detect packet loss, reordering, or duplication
- **Host behavior**: Increment SEQ for each command
- **Device behavior**: Increment SEQ for each response/event, use request SEQ in direct responses

### Message ID (MSG_ID / UUID)

- **Format**: RFC 4122 v4 UUID (128-bit, 16 bytes)
- **Generation**: Random UUID for each command
- **Correlation**: Device copies MSG_ID from command to corresponding response
- **Async events**: Device generates new UUID for unsolicited events
- **Purpose**: Match responses to requests in async environments

**UUID Structure (RFC 4122 v4)**:
```
  Byte:  0  1  2  3    4  5    6  7    8  9   10 11 12 13 14 15
Format: xxxxxxxx-xxxx-4xxx-yxxx-xxxxxxxxxxxx
         time_low  mid  hi   clk  node

  Byte 6: 0x4X (version 4)
  Byte 8: 0x8X, 0x9X, 0xAX, or 0xBX (variant bits)
```

### Timestamp Header (8 bytes at offset 502-509)

**Structure** (big-endian):
```c
typedef struct __attribute__((packed)) {
    uint32_t unix_timestamp;    // [0-3]  UTC seconds since 1970
    uint16_t milliseconds;      // [4-5]  Subseconds 0-999
    int8_t   timezone_offset;   // [6]    Timezone in 30-min units (-48 to +56)
    uint8_t  flags;             // [7]    Control flags
} timestamp_header_t;
```

**Timezone Calculation**:
- Units: 30-minute increments
- Example: UTC-6 = `-12` (6 hours × 2)
- Example: UTC+5:30 = `11` (5.5 hours × 2)
- Range: -24 hours to +28 hours

**Timestamp Flags**:
```c
#define TS_FLAG_VALID       0x01  // Timestamp is valid
#define TS_FLAG_AUTO_ADJUST 0x02  // Auto-adjust clock if drift > 1s
#define TS_FLAG_SYNC_RTC    0x04  // Sync RTC hardware
#define TS_FLAG_DST_ACTIVE  0x08  // Daylight Saving Time active
```

**Purpose**:
- Time correlation between host and device
- Automatic clock drift correction (when AUTO_ADJUST flag set)
- Round-trip latency measurement
- Event timestamping

## Payload Structures

### CMD_PING (0x01)
**Payload**: Empty (0 bytes)
**Response**: RSP_ACK with empty payload

### CMD_SET_FREQ (0x02)
**Payload** (4 bytes):
```c
typedef struct {
    float frequency_mhz;  // Frequency in MHz (e.g., 929.6625)
} cmd_set_freq_payload_t;
```
**Wire format**:
```
[0-3]  float  frequency_mhz (IEEE 754 single-precision)
```

### CMD_SET_POWER (0x03)
**Payload** (1 byte):
```c
typedef struct {
    int8_t power_dbm;  // Transmit power in dBm (-17 to +20)
} cmd_set_power_payload_t;
```
**Wire format**:
```
[0]  int8_t  power_dbm
```

### CMD_SEND_RAW (0x04)
**Payload** (variable, max 480 bytes):
```c
typedef struct {
    uint16_t data_len;        // Length of FLEX data
    uint8_t flex_data[478];   // Pre-encoded FLEX bitstream
} cmd_send_raw_payload_t;
```
**Wire format**:
```
[0-1]    uint16_t  data_len (little-endian)
[2-479]  uint8_t[] flex_data (data_len bytes)
```

### CMD_SEND_MSG (0x05)
**Payload** (variable, max 480 bytes):
```c
typedef struct {
    uint64_t capcode;         // FLEX capcode (up to 10-digit decimal)
    uint8_t message_len;      // Message text length
    char message[471];        // Message text (UTF-8, max 248 chars)
} cmd_send_msg_payload_t;
```
**Wire format**:
```
[0-7]    uint64_t  capcode (little-endian)
[8]      uint8_t   message_len
[9-479]  char[]    message (message_len bytes, UTF-8)
```
**Note**: Device performs FLEX encoding internally. Max message length: 248 characters (truncated with "..." if longer). Supports full FLEX capcode range up to 4,297,068,542.

### CMD_GET_STATUS (0x06)
**Payload**: Empty (0 bytes)
**Response**: RSP_STATUS

### CMD_SET_BAUD (0x07)
**Payload** (4 bytes):
```c
typedef struct {
    uint32_t baud_rate;  // New baud rate (e.g., 115200, 921600)
} cmd_set_baud_payload_t;
```
**Wire format**:
```
[0-3]  uint32_t  baud_rate (little-endian)
```
**Note**: Device responds with RSP_ACK, then waits 100ms before changing baud rate.

### CMD_RESET (0x08)
**Payload**: Empty (0 bytes)
**Response**: RSP_ACK, then device performs software reset

### CMD_GET_VERSION (0x09)
**Payload**: Empty (0 bytes)
**Response**: RSP_VERSION

### CMD_SET_MAILDROP (0x0A)
**Payload** (1 byte):
```c
typedef struct {
    uint8_t maildrop;  // 0 = normal, 1 = maildrop/urgent
} cmd_set_maildrop_payload_t;
```
**Wire format**:
```
[0]  uint8_t  maildrop (boolean)
```
**Note**: Affects only the next transmitted message, then auto-resets to 0.

### CMD_SET_TIME (0x0B)
**Payload** (4 bytes):
```c
typedef struct {
    uint32_t unix_timestamp;  // Unix time (seconds since epoch)
} cmd_set_time_payload_t;
```
**Wire format**:
```
[0-3]  uint32_t  unix_timestamp (little-endian)
```
**Note**: Synchronizes device RTC for event timestamps.

---

### RSP_ACK (0x81)
**Payload**: Empty (0 bytes) or optional status code
**Meaning**: Command accepted and executed successfully

### RSP_NAK (0x82)
**Payload** (1 byte):
```c
typedef struct {
    uint8_t error_code;  // Reason for rejection
} rsp_nak_payload_t;
```
**Error codes**:
```c
#define NAK_INVALID_MAGIC    0x01  // Bad packet magic
#define NAK_INVALID_VERSION  0x02  // Unsupported protocol version
#define NAK_INVALID_CRC      0x03  // CRC mismatch
#define NAK_INVALID_OPCODE   0x04  // Unknown opcode
#define NAK_INVALID_PAYLOAD  0x05  // Malformed payload
#define NAK_QUEUE_FULL       0x06  // TX queue full
#define NAK_RADIO_BUSY       0x07  // Radio busy
#define NAK_INTERNAL_ERROR   0x08  // Internal device error
```

### RSP_STATUS (0x83)
**Payload** (variable):
```c
typedef struct {
    uint8_t state;              // Device state
    float frequency_mhz;        // Current frequency
    int8_t power_dbm;           // Current power
    uint8_t queue_depth;        // TX queue depth
    uint32_t uptime_seconds;    // Uptime since boot
    uint16_t free_heap_kb;      // Free heap memory (KB)
} rsp_status_payload_t;
```
**Wire format**:
```
[0]      uint8_t   state
[1-4]    float     frequency_mhz
[5]      int8_t    power_dbm
[6]      uint8_t   queue_depth
[7-10]   uint32_t  uptime_seconds
[11-12]  uint16_t  free_heap_kb
```

**Device states**:
```c
#define STATE_IDLE        0x00  // Idle, ready for commands
#define STATE_TX          0x01  // Transmitting
#define STATE_ERROR       0x02  // Error state
#define STATE_INIT        0x03  // Initializing
```

### RSP_VERSION (0x84)
**Payload** (variable):
```c
typedef struct {
    uint8_t major;              // Major version
    uint8_t minor;              // Minor version
    uint8_t patch;              // Patch version
    char build_date[16];        // Build date (YYYY-MM-DD)
    char git_hash[8];           // Git short hash
} rsp_version_payload_t;
```
**Wire format**:
```
[0]      uint8_t  major
[1]      uint8_t  minor
[2]      uint8_t  patch
[3-18]   char[16] build_date (null-terminated)
[19-26]  char[8]  git_hash (null-terminated)
```

### RSP_ERROR (0x85)
**Payload** (variable):
```c
typedef struct {
    uint8_t error_code;         // Error code
    uint8_t details_len;        // Length of details string
    char details[478];          // Error details (UTF-8)
} rsp_error_payload_t;
```
**Wire format**:
```
[0]      uint8_t  error_code
[1]      uint8_t  details_len
[2-479]  char[]   details (details_len bytes, UTF-8)
```

---

### EVT_TX_START (0xC1)
**Payload** (20 bytes):
```c
typedef struct {
    uint8_t msg_uuid[16];       // UUID of message being transmitted
    uint32_t capcode;           // FLEX capcode
} evt_tx_start_payload_t;
```
**Wire format**:
```
[0-15]   uint8_t[16]  msg_uuid
[16-19]  uint32_t     capcode
```

### EVT_TX_COMPLETE (0xC2)
**Payload** (20 bytes):
```c
typedef struct {
    uint8_t msg_uuid[16];       // UUID of completed message
    uint32_t duration_ms;       // Transmission duration (milliseconds)
} evt_tx_complete_payload_t;
```
**Wire format**:
```
[0-15]   uint8_t[16]  msg_uuid
[16-19]  uint32_t     duration_ms
```

### EVT_TX_QUEUED (0xC3)
**Payload** (17 bytes):
```c
typedef struct {
    uint8_t msg_uuid[16];       // UUID of queued message
    uint8_t queue_position;     // Position in queue (0 = next)
} evt_tx_queued_payload_t;
```
**Wire format**:
```
[0-15]  uint8_t[16]  msg_uuid
[16]    uint8_t      queue_position
```

### EVT_ERROR (0xC4)
**Payload** (variable):
```c
typedef struct {
    uint8_t error_code;         // Error code
    uint8_t details_len;        // Length of details string
    char details[478];          // Error details (UTF-8)
} evt_error_payload_t;
```
**Wire format** (same as RSP_ERROR):
```
[0]      uint8_t  error_code
[1]      uint8_t  details_len
[2-479]  char[]   details (details_len bytes)
```

### EVT_READY (0xC5)
**Payload** (1 byte):
```c
typedef struct {
    uint8_t ready_state;  // 0x01 = ready, 0x00 = not ready
} evt_ready_payload_t;
```
**Wire format**:
```
[0]  uint8_t  ready_state
```
**Note**: Sent automatically after device boot and initialization.

### EVT_LOG (0xC6)
**Payload** (variable):
```c
typedef struct {
    uint8_t log_level;          // Log level (DEBUG/INFO/WARN/ERROR)
    uint8_t message_len;        // Length of log message
    char message[478];          // Log message (UTF-8)
} evt_log_payload_t;
```
**Wire format**:
```
[0]      uint8_t  log_level
[1]      uint8_t  message_len
[2-479]  char[]   message (message_len bytes)
```

**Log levels**:
```c
#define LOG_DEBUG  0x01
#define LOG_INFO   0x02
#define LOG_WARN   0x03
#define LOG_ERROR  0x04
```

## COBS Framing

See [COBS_ENCAPSULATION.md](COBS_ENCAPSULATION.md) for complete framing specification.

**Summary**:
- Raw packet: 512 bytes
- COBS overhead: +2 bytes worst-case
- On-wire format: `0x00 <COBS-encoded-packet> 0x00`
- Maximum on-wire size: 514 bytes

## CRC16-CCITT

**Algorithm**: CRC16-CCITT with polynomial 0x1021, initial value 0xFFFF
**Coverage**: Entire packet from MAGIC[0] through PAYLOAD[479]
**Location**: Bytes [510-511] (last 2 bytes of packet)
**Byte order**: Little-endian

**C Implementation**:
```c
uint16_t crc16_ccitt(const uint8_t *data, size_t len) {
    uint16_t crc = 0xFFFF;
    for (size_t i = 0; i < len; i++) {
        crc ^= (uint16_t)data[i] << 8;
        for (uint8_t bit = 0; bit < 8; bit++) {
            if (crc & 0x8000)
                crc = (crc << 1) ^ 0x1021;
            else
                crc <<= 1;
        }
    }
    return crc;
}
```

## Communication Patterns

### 1. Command-Response (Synchronous)

```
Host                           Device
  |                              |
  |--- CMD (UUID=A, SEQ=1) ----->|
  |                              | [process command]
  |<--- RSP (UUID=A, SEQ=2) -----|
  |                              |
```

**Host behavior**:
1. Generate random UUID for command
2. Increment SEQ
3. Send command packet
4. Wait for response with matching UUID
5. Validate CRC, match UUID

**Device behavior**:
1. Receive command
2. Validate CRC, magic, version
3. Execute command
4. Copy UUID from command to response
5. Increment SEQ
6. Send response

### 2. Asynchronous Events

```
Host                           Device
  |                              |
  |                              | [event occurs]
  |<--- EVT (UUID=B, SEQ=3) -----|
  |                              |
```

**Device behavior**:
1. Generate new random UUID for event
2. Increment SEQ
3. Send event packet

**Host behavior**:
1. Receive event
2. Validate CRC
3. Process event (UUID is independent)

### 3. Message Transmission Flow

```
Host                                    Device
  |                                       |
  |--- CMD_SEND_MSG (UUID=C, SEQ=4) ----->|
  |                                       | [validate]
  |<--- RSP_ACK (UUID=C, SEQ=5) ----------|
  |                                       | [queue message]
  |<--- EVT_TX_QUEUED (UUID=C, SEQ=6) ----|
  |                                       | [start TX]
  |<--- EVT_TX_START (UUID=C, SEQ=7) -----|
  |                                       | [transmit]
  |<--- EVT_TX_COMPLETE (UUID=C, SEQ=8) --|
  |                                       |
```

**Message UUID correlation**:
- Host generates UUID (C) in CMD_SEND_MSG
- Device copies UUID to RSP_ACK
- Device uses same UUID in EVT_TX_QUEUED, EVT_TX_START, EVT_TX_COMPLETE
- Host tracks message lifecycle via UUID

## Dual-Mode Detection (AT vs Binary)

Device firmware supports both AT command mode and binary protocol mode on the same UART:

**AT Mode Detection**:
- Packet starts with ASCII 'A' (0x41)
- Second byte is ASCII 'T' (0x54)
- Followed by optional '+' or '\\r'

**Binary Mode Detection**:
- Packet starts with magic bytes 0x46 0x4C ('FL')

**Fallback**:
- Unknown packets → ignore or send NAK
- Mode switches seamlessly per-packet

## Error Handling

### Device-side errors

1. **Invalid magic**: Send RSP_NAK with error code NAK_INVALID_MAGIC
2. **CRC mismatch**: Send RSP_NAK with error code NAK_INVALID_CRC
3. **Unknown opcode**: Send RSP_NAK with error code NAK_INVALID_OPCODE
4. **Payload validation failure**: Send RSP_NAK with error code NAK_INVALID_PAYLOAD
5. **Internal error**: Send RSP_ERROR or EVT_ERROR with details

### Host-side errors

1. **CRC mismatch**: Discard packet, log error
2. **UUID mismatch**: Timeout waiting for correct response, retry
3. **Timeout**: Retry command (max 3 attempts)
4. **NAK received**: Check error code, handle accordingly

## Implementation Notes

### Message Truncation (v2.5.4+)

- Max message length: 248 characters (UTF-8)
- Longer messages: Truncated at 245 chars + "..." = 248 total
- Response indicates truncation via flag or field

### EMR (Emergency Message Resynchronization) v2.5.4+

- Sync burst sent before first message or after 10-minute idle
- Pattern: {0xA5, 0x5A, 0xA5, 0x5A} at current radio settings
- Improves pager synchronization

### Queue Management

- Device maintains TX queue (depth reported in RSP_STATUS)
- Queue full → RSP_NAK with NAK_QUEUE_FULL
- Queue processed FIFO

### Timestamp Synchronization

- Host sends CMD_SET_TIME to sync device RTC
- Device timestamps all events if RTC synchronized
- Check FLAG_TIMESTAMP_VALID before trusting timestamp

## Versioning

**Current version**: 2.5.4

**Version history**:
- v2.5.1: Initial binary protocol with COBS/CRC16
- v2.5.2: Added UUID support for message correlation (128-bit RFC 4122 v4)
- v2.5.3: Added 8-byte timestamp header at offset 502-509, reduced payload from 486 to 478 bytes
- v2.5.4: Increased payload to 480 bytes, added AT+CCLK command, EMR sync, message truncation

---

**See also**:
- [COBS_ENCAPSULATION.md](COBS_ENCAPSULATION.md) - COBS framing details
- [AT_COMMANDS.md](AT_COMMANDS.md) - Legacy AT command reference
