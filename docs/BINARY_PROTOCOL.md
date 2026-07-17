# Binary Protocol Specification v2.5.7

## Overview

Binary protocol used by FlexDevice over UART with COBS framing and CRC16-CCITT integrity checks.

- Current firmware target: `v2.5.7`
- Transport: UART `115200 8N1`
- Framing: COBS, frame delimiter `0x00`

## Packet Layout

All packets are exactly 512 bytes, COBS-encoded for wire transport.

```text
Offset    Size  Field
------    ----  -------------------------------
[0]       1     type       (CMD/RSP/EVT)
[1]       1     opcode
[2]       1     flags
[3-18]    16    uuid       (RFC4122 v4 bytes)
[19-20]   2     payload_len (big-endian)
[21-501]  481   payload
[501-508] 8     timestamp header
[509-510] 2     crc16      (little-endian storage)
```

### Constants

```c
#define PACKET_FIXED_SIZE      512
#define PACKET_HEADER_SIZE     21
#define PACKET_PAYLOAD_SIZE    481
#define PACKET_TIMESTAMP_SIZE  8
#define PACKET_CRC_OFFSET      510
```

## Packet Types

```c
#define PKT_TYPE_CMD 0x01
#define PKT_TYPE_RSP 0x02
#define PKT_TYPE_EVT 0x03
```

## Opcodes

### Commands (Host -> Device)

```c
#define CMD_SEND_FLEX     0x01
#define CMD_GET_STATUS    0x02
#define CMD_ABORT         0x03
#define CMD_SET_CONFIG    0x04  // currently returns NACK
#define CMD_GET_CONFIG    0x05  // currently returns NACK
#define CMD_PING          0x06
#define CMD_GET_LOGS      0x07  // currently returns NACK
#define CMD_CLEAR_LOGS    0x08  // currently returns NACK
#define CMD_FACTORY_RESET 0x09  // currently returns NACK
```

### Responses (Device -> Host)

```c
#define RSP_ACK    0x01
#define RSP_NACK   0x02
#define RSP_STATUS 0x03
#define RSP_CONFIG 0x04
#define RSP_PONG   0x05
#define RSP_LOGS   0x06
```

### Events (Device -> Host, async)

```c
#define EVT_TX_QUEUED          0x01
#define EVT_TX_START           0x02
#define EVT_TX_DONE            0x03
#define EVT_TX_FAILED          0x04
#define EVT_BOOT               0x05
#define EVT_ERROR              0x06
#define EVT_BATTERY_LOW        0x07
#define EVT_POWER_DISCONNECTED 0x08
```

## Flags

Control flags in packet byte `[2]`:

```c
#define FLAG_ACK_REQUIRED  (1 << 0)
#define FLAG_RETRY         (1 << 1)
#define FLAG_ERROR         (1 << 2)
#define FLAG_PRIORITY      (1 << 3)
#define FLAG_FRAGMENTED    (1 << 4)
#define FLAG_LAST_FRAGMENT (1 << 5)
```

Timestamp flags in timestamp byte `[509]`:

```c
#define TS_FLAG_VALID       0x01
#define TS_FLAG_AUTO_ADJUST 0x02
#define TS_FLAG_SYNC_RTC    0x04
#define TS_FLAG_DST_ACTIVE  0x08
```

## UUID

- UUID is 16 raw bytes (RFC4122 v4 layout).
- For command/response, device copies command UUID into response UUID.
- TX lifecycle events reuse the same UUID as the queued message.

## Timestamp Header (offset 501-508)

```c
typedef struct __attribute__((packed)) {
    uint32_t unix_timestamp;    // big-endian
    uint16_t milliseconds;      // big-endian
    int8_t   timezone_offset;   // 30-minute units
    uint8_t  flags;
} timestamp_header_t;
```

Notes:
- Host may set `TS_FLAG_AUTO_ADJUST` and `TS_FLAG_SYNC_RTC`.
- Device sets outgoing `TS_FLAG_VALID` only when system time is initialized.

## Payload Definitions

### CMD_SEND_FLEX (`0x01`)

```text
[0-7]    uint64_t capcode      (little-endian)
[8-11]   float    frequency_mhz (IEEE754, little-endian)
[12]     int8_t   tx_power
[13]     uint8_t  mail_drop
[14]     uint8_t  msg_len
[15..]   bytes    message
```

- Protocol `msg_len` supports up to `255` bytes.
- Firmware applies FLEX operational limit of `248` chars and truncates when needed.

### CMD_GET_STATUS (`0x02`)

- Empty payload.

### CMD_ABORT (`0x03`)

- Empty payload.

### CMD_PING (`0x06`)

- Empty payload.

### RSP_ACK / RSP_NACK payload (`0x01` / `0x02`)

```text
[0] uint8_t status
```

Status values:

```c
#define STATUS_ACCEPTED      0x00
#define STATUS_REJECTED      0x01
#define STATUS_QUEUE_FULL    0x02
#define STATUS_INVALID_PARAM 0x03
#define STATUS_BUSY          0x04
#define STATUS_ERROR         0x05
```

### RSP_STATUS (`0x03`)

```text
[0]    uint8_t  device_state
[1]    uint8_t  queue_count
[2]    uint8_t  battery_pct
[3-4]  uint16_t battery_mv      (little-endian)
[5-8]  float    frequency_mhz   (little-endian)
[9]    int8_t   power_dbm
```

### RSP_PONG (`0x05`)

- Empty payload.

### Events in use

- `EVT_TX_QUEUED`: payload `[0] queue_pos`
- `EVT_TX_START`: empty payload
- `EVT_TX_DONE`: payload `[0] result`
- `EVT_TX_FAILED`: payload `[0] error_code`

Result codes:

```c
#define RESULT_SUCCESS        0x00
#define RESULT_RADIO_ERROR    0x01
#define RESULT_ENCODING_ERROR 0x02
#define RESULT_TIMEOUT        0x03
#define RESULT_ABORTED        0x04
```

## COBS Framing

- Encode the full 512-byte packet.
- Transmit `<COBS_ENCODED_BYTES ... 0x00>`.
- Max encoded size is typically 513 bytes.

See `docs/COBS_ENCAPSULATION.md` for algorithm details and vectors.

## CRC16-CCITT

- Polynomial: `0x1021`
- Init: `0xFFFF`
- Coverage: packet bytes `[0..509]`
- Stored at bytes `[510..511]`

## Supported Interaction Flows

### 1) Command-response

1. Host sends CMD packet (with UUID).
2. Device validates COBS decode + size + CRC.
3. Device executes handler and replies with RSP carrying same UUID.

### 2) Message TX lifecycle

1. Host sends `CMD_SEND_FLEX`.
2. Device returns `RSP_ACK` (`STATUS_ACCEPTED`) or `RSP_NACK`.
3. Device emits `EVT_TX_QUEUED`.
4. Device emits `EVT_TX_START`.
5. Device emits `EVT_TX_DONE` or `EVT_TX_FAILED`.

## Version Notes

- `v2.5.5` introduced 8-byte capcode (`uint64_t`) in `CMD_SEND_FLEX`.
- `v2.5.6` improved binary stream reliability (larger UART TX buffer, fewer mixed-stream issues).
- `v2.5.7` added per-frame reception timeout to prevent AT lockout after partial binary frames.

## Protocol Hardening (Implemented in v2.5.7)

### Fix — Per-Frame Reception Timeout

- **Status**: Implemented.
- **Firmware** (`at_commands.cpp`): `process_binary_frame()` tracks `binary_frame_start_ms` on the first byte. If the frame is not completed within `BINARY_FRAME_TIMEOUT_MS` (200 ms), the partial buffer is discarded and the state resets.
- **Constant**: `BINARY_FRAME_TIMEOUT_MS = 200` in `config.h`. Derived from: 518 bytes at 115200 baud ≈ 45 ms; 200 ms = 4.4× safety margin.

## Related Docs

- `docs/COBS_ENCAPSULATION.md`
- `docs/AT_COMMANDS.md`
- `README.md`
