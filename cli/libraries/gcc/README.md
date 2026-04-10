# libflex_binary — C Library for FLEX Binary Protocol

Single-header C library for FLEX-FSK-TX v2.5 binary protocol over UART.

## Architecture

FlexDevice.h is a header-only library implementing the complete binary protocol client stack:

```
Application Code
       ↓
Public API (flex_send_msg, flex_ping, flex_get_status)
       ↓
Packet Builder (_flex_build_packet)
       ↓
COBS Encoder (_flex_cobs_encode)
       ↓
UART TX → [DEVICE] → UART RX
       ↓
Frame Reader (_flex_read_frame)
       ↓
COBS Decoder (_flex_cobs_decode)
       ↓
CRC Validator (_flex_crc16)
       ↓
Packet Parser (_flex_parse_packet)
       ↓
Response Matcher (UUID correlation)
       ↓
Public API returns
```

## Library Components

### 1. Core Data Structures

**FlexDevice** (public):
```c
typedef struct {
    int      fd;       // POSIX file descriptor for serial port
    uint8_t  seq;      // Auto-incrementing sequence counter (0-255, wraps)
    int      verbose;  // Debug output flag
} FlexDevice;
```

**flex_packet_t** (internal):
```c
typedef struct {
    uint8_t  pkt_type;       // CMD/RSP/EVT
    uint8_t  opcode;
    uint8_t  flags;
    uint8_t  seq;
    uint8_t  uuid[16];       // RFC 4122 v4 UUID
    uint16_t payload_len;
    uint8_t  payload[480];
    uint32_t ts_unix;        // Unix timestamp (seconds)
    uint16_t ts_ms;          // Milliseconds
    int8_t   ts_tz;          // Timezone offset (30-min units)
    uint8_t  ts_flags;       // VALID/AUTO_ADJUST/SYNC_RTC/DST
    uint16_t crc16;
} flex_packet_t;
```

### 2. Packet Building

**_flex_build_packet()**:
- Allocates 512-byte raw packet buffer
- Writes magic bytes 0x46 0x4C ('FL')
- Sets type, opcode, flags, seq
- Copies 16-byte UUID
- Writes payload length (big-endian uint16 at offset 20)
- Copies payload data (max 480 bytes at offset 22)
- **Timestamp** (offset 502-509, 8 bytes):
  - [502-505]: unix timestamp (big-endian uint32)
  - [506-507]: milliseconds (big-endian uint16)
  - [508]: timezone offset in 30-minute units (int8)
  - [509]: flags byte (VALID | AUTO_ADJUST | SYNC_RTC | DST)
- Computes CRC16-CCITT over bytes [0-509]
- Stores CRC at offset 510-511 (little-endian uint16)

**Payload Layouts**:

CMD_SEND_FLEX (11 fixed bytes + message):
```
[0-3]   capcode      (little-endian uint32)
[4-7]   frequency    (IEEE 754 float, little-endian)
[8]     tx_power     (int8)
[9]     mail_drop    (uint8, 0/1)
[10]    msg_len      (uint8)
[11+]   message      (UTF-8 text, max 255 bytes)
```

CMD_PING: Empty payload

CMD_GET_STATUS: Empty payload

### 3. UUID Generation

**_flex_uuid_v4()**:
- Reads 16 random bytes from `/dev/urandom`
- Sets version bits: `uuid[6] = (uuid[6] & 0x0F) | 0x40` (v4)
- Sets variant bits: `uuid[8] = (uuid[8] & 0x3F) | 0x80` (RFC 4122)
- Fallback: zeros on `/dev/urandom` failure

**_flex_uuid_to_str()**:
- Formats to canonical string: `xxxxxxxx-xxxx-4xxx-yxxx-xxxxxxxxxxxx`
- Used for return values and debug output

### 4. COBS Framing

**_flex_cobs_encode(input[512]) → output[max 514]**:
- Replaces all 0x00 bytes with overhead codes
- Each non-zero run is prefixed with its length
- Special case: 254-byte run → code 0xFF (no zero inserted)
- Appends 0x00 delimiter
- Overhead: +1 byte per 254-byte block worst-case

**_flex_cobs_decode(input) → output[512]**:
- Reads code bytes, copies data, inserts zeros
- Validates: input must end with 0x00, no embedded zeros allowed
- Returns decoded length or 0 on error

**Wire format**: `[COBS-encoded packet (513-514 bytes)] 0x00`

### 5. CRC16-CCITT

**_flex_crc16(data, len)**:
- Polynomial: 0x1021
- Init: 0xFFFF
- Table-driven algorithm (256-entry lookup table)
- No final XOR, no bit reflection
- Covers raw packet bytes [0-509]
- Stored at [510-511] as little-endian uint16

Test vector: `"123456789"` → `0x29B1`

### 6. Serial Port Management

**flex_open(device, baudrate)**:
- Opens device with `O_RDWR | O_NOCTTY | O_NONBLOCK`
- Configures termios: 8N1, no flow control, raw mode
- Supported baud rates: 9600, 19200, 38400, 57600, 115200, 230400, 460800, 921600
- Flushes buffers with `tcflush(TCIOFLUSH)`
- Initializes `seq = 1`, `fd = descriptor`, `verbose = 0`

**flex_close()**:
- Closes file descriptor
- Sets `fd = -1`

### 7. Frame Reception

**_flex_read_frame(timeout_ms) → frame_length**:
- **Mixed ASCII/Binary stream handling**:
  - Accumulates bytes until 0x00 delimiter
  - Detects ASCII lines (printable + `\n`) and prints them as "DEVICE: ..."
  - Switches to binary mode on first non-printable, non-newline byte
  - Returns COBS frame (including 0x00) when delimiter found
- **Timeout**: Uses `gettimeofday()` for elapsed time check
- **Non-blocking reads**: Retries with 1ms sleep on EAGAIN/EWOULDBLOCK
- Returns frame length on success, -1 on timeout

### 8. Packet Parsing

**_flex_parse_packet(raw[512])**:
- Validates CRC (compare packet[510-511] with computed CRC)
- Extracts fields:
  - Type/opcode/flags/seq: bytes [0-3]
  - UUID: bytes [4-19]
  - Payload length: bytes [20-21] (big-endian uint16)
  - Payload: bytes [22 to 22+len]
  - Timestamp: bytes [502-509] (big-endian multi-field)
  - CRC: bytes [510-511] (little-endian uint16)
- Populates `flex_packet_t` struct
- Returns 0 on success, -1 on CRC mismatch

### 9. Timestamp Handling

**_flex_populate_ts(raw[512])**:
- Calls `gettimeofday()` for current UTC time
- Computes:
  - `unix_ts` = seconds since epoch
  - `millis` = microseconds / 1000
  - `tz_offset` = local timezone in 30-minute units (uses `localtime_r`)
  - `flags` = VALID | AUTO_ADJUST | SYNC_RTC
- Writes 8-byte timestamp block at offset 502-509 (big-endian)

### 10. High-Level API

**flex_send_msg()**:
1. Generate RFC 4122 v4 UUID (16 bytes)
2. Increment `dev->seq` (wraps at 255)
3. Build CMD_SEND_FLEX packet with capcode/frequency/power/message payload
4. Encode with COBS
5. Write to UART
6. Read frames until RSP_ACK with matching UUID (timeout 5000ms)
7. Validate status byte in ACK payload[0]
8. Return 0 if ACCEPTED, -1 otherwise
9. Optionally copy UUID to caller's 37-byte buffer

**flex_send_msg_wait()**:
- Same as `flex_send_msg()` but also waits for EVT_TX_DONE
- Reads additional frames until EVT_TX_DONE/EVT_TX_FAILED with matching UUID
- Returns 0 on RESULT_SUCCESS, -1 on failure

**flex_ping()**:
1. Generate UUID
2. Send CMD_PING packet
3. Wait for RSP_PONG with matching UUID (timeout 5000ms)
4. Return 0 on success, -1 on timeout

**flex_get_status()**:
1. Generate UUID
2. Send CMD_GET_STATUS packet
3. Wait for RSP_STATUS with matching UUID
4. Parse payload:
   - [0]: device_state (IDLE/TX/ERROR/INIT)
   - [1]: queue_count (uint8)
   - [2]: battery_pct (uint8, 0-100)
   - [3-4]: battery_mv (little-endian uint16)
   - [5-8]: frequency (IEEE 754 float, little-endian)
   - [9]: power (int8, dBm)
5. Fill caller's output pointers (NULL pointers ignored)
6. Return 0 on success, -1 on error

### 11. Response Correlation

**UUID Matching**:
- Commands generate random UUID
- Device copies UUID from command to response
- Library waits for response where `rsp_uuid == cmd_uuid`
- Async events have independent UUIDs (ignored during command/response matching)
- Timeout if no matching response within 5000ms

**Sequence Numbers**:
- Not used for correlation (UUID is authoritative)
- Incremented per packet for debugging/packet loss detection

## Protocol Constants

All constants mirror firmware's `binary_packet.h`:

```c
#define FLEX_PACKET_SIZE      512    // Total packet size
#define FLEX_HEADER_SIZE       22    // Bytes before payload
#define FLEX_PAYLOAD_SIZE     480    // Max payload bytes
#define FLEX_CRC_OFFSET       510    // CRC location
#define FLEX_TS_OFFSET        502    // Timestamp location
#define FLEX_MAX_MESSAGE      248    // Firmware truncates to 248 chars
#define FLEX_MAX_MSG_PROTO    255    // Protocol max (before truncation)
```

## Usage Example

```c
#include "FlexDevice.h"

int main(void) {
    FlexDevice dev;

    // Open serial port
    if (flex_open(&dev, "/dev/ttyUSB0", 115200) < 0) {
        fprintf(stderr, "Failed to open device\n");
        return 1;
    }

    // Optional: enable debug output
    dev.verbose = 1;

    // Send message and get UUID
    char uuid[37];
    if (flex_send_msg(&dev, 1234567, 931.9375, 10, 0, "Hello World", uuid) == 0) {
        printf("Message sent. UUID: %s\n", uuid);
    } else {
        fprintf(stderr, "Failed to send message\n");
    }

    // Get device status
    uint8_t state, queue, bat_pct;
    uint16_t bat_mv;
    float freq;
    int8_t power;

    if (flex_get_status(&dev, &state, &queue, &bat_pct, &bat_mv, &freq, &power) == 0) {
        printf("State=%d Queue=%d Battery=%d%% (%.3fV) Freq=%.4f Power=%d\n",
               state, queue, bat_pct, bat_mv / 1000.0, freq, power);
    }

    // Test connectivity
    if (flex_ping(&dev) == 0) {
        printf("Device is alive\n");
    }

    // Wait for transmission to complete
    if (flex_send_msg_wait(&dev, 1234567, 931.9375, 10, 0, "Test", uuid, 30) == 0) {
        printf("Transmission completed successfully\n");
    }

    flex_close(&dev);
    return 0;
}
```

## Compilation

Header-only library — no separate compilation required.

```bash
gcc -o myapp myapp.c -O2 -Wall
```

Dependencies: POSIX (termios, unistd, fcntl)

## Thread Safety

**Not thread-safe**. Use one FlexDevice instance per thread, or add external mutex.

Reasons:
- Shared `seq` counter
- Single `fd` for RX/TX
- No internal locking

## Error Handling

Functions return:
- `0` = success
- `-1` = error

Error sources:
- Serial port failure (check errno)
- Timeout (no matching response)
- CRC mismatch (corrupt packet)
- NAK or rejected status from device

Debug with `dev.verbose = 1` to see packet hex dumps and device ASCII output.

## See Also

- [../docs/BINARY_PROTOCOL.md](../docs/BINARY_PROTOCOL.md) — Protocol specification
- [../docs/COBS_ENCAPSULATION.md](../docs/COBS_ENCAPSULATION.md) — Framing details
- [example.c](example.c) — Complete CLI example
