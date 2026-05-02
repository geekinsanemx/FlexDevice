# FlexDevice.py — Python Library for FLEX Binary Protocol

Python module for FLEX-FSK-TX v2.5 binary protocol over UART.

**Version:** v2.5.6 (compatible with firmware v2.5.5+)
**Note:** Firmware v2.5.6+ silences ASCII logs during binary sessions, so host readers can treat the stream as deterministic frames.

## Architecture

FlexDevice.py implements the complete binary protocol client stack in pure Python:

```
Application Code
       ↓
Public API (send_message, ping, get_status)
       ↓
Packet Builder (_build_packet, _build_cmd_send_flex)
       ↓
COBS Encoder (_cobs_encode)
       ↓
pyserial write() → [DEVICE] → pyserial read()
       ↓
Frame Reader (_read_frame)
       ↓
COBS Decoder (_cobs_decode)
       ↓
CRC Validator (_crc16)
       ↓
Packet Parser (_parse_packet)
       ↓
Response Matcher (UUID correlation)
       ↓
Public API returns
```

## Library Components

### 1. Protocol Constants

All constants mirror firmware's `binary_packet.h`:

```python
PACKET_FIXED_SIZE    = 512   # Total packet size
PACKET_PAYLOAD_SIZE  = 481   # Max payload bytes
PACKET_CRC_OFFSET    = 510   # CRC location
PACKET_TS_OFFSET     = 501   # Timestamp location
MAX_MESSAGE_PROTO    = 255   # Protocol max (firmware truncates to 248)
CMD_SEND_ARGS_SIZE   = 15    # Fixed payload size for CMD_SEND_FLEX

# Packet types
PKT_TYPE_CMD = 0x01  # Command (host → device)
PKT_TYPE_RSP = 0x02  # Response (device → host)
PKT_TYPE_EVT = 0x03  # Event (device → host, async)

# Commands
CMD_SEND_FLEX  = 0x01  # Send FLEX message
CMD_GET_STATUS = 0x02  # Query device status
CMD_PING       = 0x06  # Ping device

# Responses
RSP_ACK    = 0x01  # Acknowledgment
RSP_NACK   = 0x02  # Negative acknowledgment
RSP_STATUS = 0x03  # Status response
RSP_PONG   = 0x05  # Pong response

# Events
EVT_TX_QUEUED  = 0x01  # Message queued
EVT_TX_START   = 0x02  # Transmission started
EVT_TX_DONE    = 0x03  # Transmission completed
EVT_TX_FAILED  = 0x04  # Transmission failed

# Status codes (ACK payload[0])
STATUS_ACCEPTED      = 0x00
STATUS_REJECTED      = 0x01
STATUS_QUEUE_FULL    = 0x02
STATUS_INVALID_PARAM = 0x03
STATUS_BUSY          = 0x04
STATUS_ERROR         = 0x05

# Result codes (EVT_TX_DONE/FAILED payload[0])
RESULT_SUCCESS        = 0x00
RESULT_RADIO_ERROR    = 0x01
RESULT_ENCODING_ERROR = 0x02
RESULT_TIMEOUT        = 0x03
RESULT_ABORTED        = 0x04

# Timestamp flags
TS_FLAG_VALID       = 0x01  # Timestamp is valid
TS_FLAG_AUTO_ADJUST = 0x02  # Auto-adjust for DST
TS_FLAG_SYNC_RTC    = 0x04  # Sync device RTC
```

### 2. Exception Hierarchy

```python
FlexError               # Base exception
├── FlexTimeoutError    # No response within timeout
├── FlexNackError       # NACK received (has .status attribute)
├── FlexRejectedError   # ACK but status != ACCEPTED (has .status attribute)
└── FlexTxFailedError   # EVT_TX_FAILED received (has .result attribute)
```

**Usage**:
```python
try:
    uuid = dev.send_message(1234567, 931.9375, 10, "Hello")
except FlexTimeoutError:
    print("No response from device")
except FlexNackError as e:
    print(f"Device rejected: status=0x{e.status:02X}")
except FlexRejectedError as e:
    print(f"Queue full or busy: status=0x{e.status:02X}")
```

### 3. Packet Building

**_build_packet(pkt_type, opcode, uuid, payload) → bytes[512]**:
- Creates 512-byte bytearray initialized to zeros
- **Structure**:
  - [0]: packet type (CMD/RSP/EVT)
  - [1]: opcode
  - [2]: flags (FLAG_ACK_REQUIRED = 0x01)
  - [3-18]: UUID (16 bytes)
  - [19-20]: payload length (big-endian uint16)
  - [21-501]: payload data (max 481 bytes)
  - [501-508]: timestamp (8 bytes, see below)
  - [510-511]: CRC16 (little-endian uint16)

**Timestamp Block** (offset 501-508):
```python
# Big-endian multi-field struct
[501-504]  uint32  unix_timestamp (seconds since epoch)
[505-506]  uint16  milliseconds (0-999)
[507]      int8    timezone_offset (in 30-minute units)
[508]      uint8   flags (VALID | AUTO_ADJUST | SYNC_RTC)
```
- Uses `datetime.now(timezone.utc)` for UTC time
- Converts local timezone offset to 30-minute units: `tz_units = -int(offset / 1800)`
- Example: UTC-6 = -360 minutes / 30 = -12 units

**CRC16 Calculation**:
- Polynomial: 0x1021 (CRC16-CCITT)
- Initial value: 0xFFFF
- Covers bytes [0-509] (entire packet except CRC field)
- Stored as little-endian uint16 at [510-511]: `struct.pack_into('<H', raw, 510, crc)`

**_build_cmd_send_flex(uuid, capcode, freq, power, mail_drop, message) → bytes[512]**:

Payload layout (little-endian):
```python
[0-7]   capcode      struct.pack('<Q', capcode)      # uint64
[8-11]  frequency    struct.pack('<f', frequency)    # IEEE 754 float
[12]    tx_power     struct.pack('b', power)         # int8
[13]    mail_drop    struct.pack('B', mail_drop)     # uint8
[14]    msg_len      struct.pack('B', len(message))  # uint8
[15+]   message      message bytes (UTF-8)
```

Returns complete 512-byte packet via `_build_packet()`.

### 4. UUID Generation

**Built-in `uuid` module** (RFC 4122 v4):
```python
import uuid as uuid_module
msg_uuid = uuid_module.uuid4().bytes  # 16-byte random UUID
```

**UUID String Formatting**:
```python
uuid_str = str(uuid_module.UUID(bytes=msg_uuid))
# Format: "xxxxxxxx-xxxx-4xxx-yxxx-xxxxxxxxxxxx"
```

- Version nibble: byte[6] bits[4-7] = 0x4
- Variant bits: byte[8] bits[6-7] = 0b10

### 5. COBS Framing

**_cobs_encode(data: bytes) → bytes**:
- Input: 512-byte packet
- Output: ~513 bytes (encoded + 0x00 delimiter)
- Algorithm:
  1. Initialize output bytearray with overhead code placeholder
  2. For each input byte:
     - If 0x00: finalize current block, start new block
     - Else: append byte, increment code
     - If code reaches 0xFF: finalize block (254-byte run)
  3. Finalize last block
  4. Append 0x00 delimiter
- Returns `bytes(output)`

**_cobs_decode(data: bytes) → bytes**:
- Validates: `data[-1] == 0x00` (must end with delimiter)
- Validates: No embedded 0x00 bytes in encoded data
- Decodes by reading code bytes and inserting zeros
- Returns raw 512-byte packet
- Raises `FlexError` on invalid frame

**Wire Format**:
```
[code_byte] [data...] [code_byte] [data...] ... [0x00]
 overhead                overhead            delimiter
```

### 6. CRC16-CCITT

**_crc16(data: bytes) → int**:
- Table-driven algorithm using 256-entry lookup table
- Implementation:
```python
crc = 0xFFFF
for b in data:
    crc = ((crc << 8) & 0xFFFF) ^ _CRC16_TABLE[((crc >> 8) ^ b) & 0xFF]
return crc
```
- No final XOR, no bit reflection
- Test vector: `b"123456789"` → `0x29B1`

### 7. Serial Port Management (pyserial)

**FlexDevice.__init__(port, baudrate=115200, timeout=5.0, verbose=False)**:
- Stores port/baudrate/timeout/verbose
- Initializes `last_response = None`
- Does NOT open serial port (call `open()` or use context manager)

**open()**:
```python
self.ser = serial.Serial(
    port=self.port,
    baudrate=self.baudrate,
    bytesize=serial.EIGHTBITS,
    parity=serial.PARITY_NONE,
    stopbits=serial.STOPBITS_ONE,
    timeout=0.05,         # Short read timeout for non-blocking
    write_timeout=2.0
)
self.ser.reset_input_buffer()
self.ser.reset_output_buffer()
```

**close()**:
```python
if self.ser and self.ser.is_open:
    self.ser.close()
```

**Context Manager**:
```python
with FlexDevice('/dev/ttyUSB0') as dev:
    dev.send_message(...)
# Automatically calls close() on exit
```

### 8. Frame Reception

**_read_frame(timeout_sec) → bytes**:
- Drains any leading ASCII (legacy firmware logs) and, if `verbose`, prints them as `"DEVICE: …"`.
- After the first non-printable byte, treats the stream purely as a COBS frame and buffers until the `0x00` delimiter.
- Uses `time.time()` for timeout tracking and retries on empty reads (`pyserial` with short timeout).
- Raises `FlexTimeoutError` if no complete frame is received within `timeout_sec`.

### 9. Packet Parsing

**_parse_packet(data: bytes) → dict**:
- Validates packet size: must be 512 bytes
- Validates CRC: extracts CRC from [510-511], computes CRC over [0-509], compares
- Raises `FlexError` on size or CRC mismatch
- Extracts fields using `struct.unpack_from()`:
```python
{
    'type':     data[0],              # uint8
    'opcode':   data[1],              # uint8
    'flags':    data[2],              # uint8
    'uuid':     data[3:19],           # bytes[16]
    'payload':  data[21:21+payload_len],  # bytes[0-481]
    'ts_unix':  struct.unpack_from('>I', data, 501)[0],  # uint32 BE
    'ts_ms':    struct.unpack_from('>H', data, 505)[0],  # uint16 BE
    'ts_tz':    struct.unpack_from('b',  data, 507)[0],  # int8
    'ts_flags': data[508],                               # uint8
}
```

### 10. High-Level API

**FlexDevice Instance Fields**:
```python
self.port         # Serial port path (e.g., '/dev/ttyUSB0')
self.baudrate     # Baud rate (default 115200)
self.timeout      # Response timeout in seconds (default 5.0)
self.verbose      # Debug output flag (default False)
self.last_response  # dict of last parsed response/event packet
```

**send_message(capcode, frequency, power, message, mail_drop=False) → uuid_str**:
1. Generate random UUID: `uuid_module.uuid4().bytes`
2. Truncate message to 255 bytes if needed
4. Build CMD_SEND_FLEX packet
5. COBS-encode and send
6. Read frames until RSP_ACK with matching UUID (timeout: `self.timeout`)
7. Validate `payload[0] == STATUS_ACCEPTED`
8. Store response in `self.last_response`
9. Return UUID string
10. Raises: `FlexTimeoutError`, `FlexNackError`, `FlexRejectedError`

**send_message_wait(capcode, frequency, power, message, mail_drop=False, wait_timeout=30.0) → uuid_str**:
- Same as `send_message()` but also waits for EVT_TX_DONE
- After RSP_ACK, continues reading frames until:
  - EVT_TX_DONE with matching UUID and `payload[0] == RESULT_SUCCESS`
  - EVT_TX_FAILED with matching UUID → raises `FlexTxFailedError`
  - Timeout after `wait_timeout` seconds → raises `FlexTimeoutError`

**ping() → rtt_ms**:
1. Generate UUID
2. Send CMD_PING packet
3. Measure round-trip time using `time.time()`
4. Wait for RSP_PONG with matching UUID
5. Return RTT in milliseconds (float)

**get_status() → dict**:
1. Generate UUID
2. Send CMD_GET_STATUS packet
3. Wait for RSP_STATUS with matching UUID
4. Parse payload:
```python
{
    'device_state': payload[0],           # uint8 (IDLE/TX/ERROR/INIT)
    'queue_count':  payload[1],           # uint8
    'battery_pct':  payload[2],           # uint8 (0-100%)
    'battery_mv':   struct.unpack_from('<H', payload, 3)[0],  # uint16
    'frequency':    struct.unpack_from('<f', payload, 5)[0],  # float
    'power':        struct.unpack_from('b',  payload, 9)[0],  # int8
}
```
5. Store in `self.last_response` and return dict

### 11. Response Correlation

**UUID Matching Logic**:
- Commands generate random 16-byte UUID
- Device copies UUID from command to response
- API methods call `_recv_packet()` and apply UUID checks in the command flow:
  1. Read frame with `_read_frame()`
  2. Decode COBS → raw packet
  3. Parse packet → dict
  4. Compare packet UUID with command UUID when required
  5. Ignore unrelated async frames
  6. Timeout raises `FlexTimeoutError`

**last_response Field**:
- Updated after every successful API call
- Contains complete parsed packet dict with all fields
- Useful for inspecting timestamps, flags, raw payload

## Usage Examples

### Basic Message Transmission

```python
from FlexDevice import FlexDevice

with FlexDevice('/dev/ttyUSB0', verbose=True) as dev:
    uuid = dev.send_message(
        capcode=1234567,
        frequency=931.9375,
        power=10,
        message="Hello World"
    )
    print(f"Message sent: {uuid}")

    # Inspect last response
    if dev.last_response['ts_flags'] & 0x01:
        ts = dev.last_response['ts_unix']
        print(f"Device timestamp: {ts}")
```

### Wait for Transmission Complete

```python
dev = FlexDevice('/dev/ttyUSB0', timeout=10.0)
dev.open()

try:
    uuid = dev.send_message_wait(
        capcode=1234567,
        frequency=929.6625,
        power=15,
        message="Important Message",
        mail_drop=True,       # Urgent flag
        wait_timeout=60.0     # Wait up to 60 sec for TX_DONE
    )
    print(f"Transmission completed: {uuid}")
except FlexTxFailedError as e:
    print(f"Transmission failed: result=0x{e.result:02X}")
finally:
    dev.close()
```

### Device Status Monitoring

```python
with FlexDevice('/dev/ttyUSB0') as dev:
    status = dev.get_status()

    print(f"Device State: {status['device_state']}")
    print(f"Queue Depth: {status['queue_count']}")
    print(f"Battery: {status['battery_pct']}% ({status['battery_mv']}mV)")
    print(f"Frequency: {status['frequency']:.4f} MHz")
    print(f"TX Power: {status['power']} dBm")
```

### Ping / Connectivity Test

```python
with FlexDevice('/dev/ttyUSB0') as dev:
    try:
        rtt = dev.ping()
        print(f"Device is alive. RTT: {rtt:.2f} ms")
    except FlexTimeoutError:
        print("Device not responding")
```

### Error Handling

```python
from FlexDevice import (FlexDevice, FlexTimeoutError, FlexNackError,
                        FlexRejectedError, FlexTxFailedError)

dev = FlexDevice('/dev/ttyUSB0', timeout=5.0, verbose=True)
dev.open()

try:
    uuid = dev.send_message(1234567, 931.9375, 10, "Test Message")
    print(f"Success: {uuid}")
except FlexTimeoutError:
    print("No response from device (check connection)")
except FlexNackError as e:
    print(f"Device sent NACK: status=0x{e.status:02X}")
except FlexRejectedError as e:
    if e.status == 0x02:  # STATUS_QUEUE_FULL
        print("Device queue is full, retry later")
    else:
        print(f"Message rejected: status=0x{e.status:02X}")
except Exception as e:
    print(f"Unexpected error: {e}")
finally:
    dev.close()
```

## Dependencies

```bash
pip3 install pyserial
```

Tested with:
- Python 3.6+
- pyserial 3.5+

## Thread Safety

**Not thread-safe**. Use one FlexDevice instance per thread or add external `threading.Lock`.

Reasons:
- Shared `pyserial.Serial` instance
- No internal locking

## Performance Notes

- **COBS overhead**: +1-2 bytes per 512-byte packet (~0.4%)
- **Packet size**: ~513 bytes on wire
- **Baud rate vs throughput**:
  - 115200 baud: ~11.5 KB/s → ~22 packets/sec
  - 921600 baud: ~92 KB/s → ~180 packets/sec
- **Typical RTT** (ping): 20-50ms at 115200 baud

## Debugging

**Enable verbose mode**:
```python
dev = FlexDevice('/dev/ttyUSB0', verbose=True)
```

Output includes:
- TX packet hex dump (first 32 bytes)
- RX packet hex dump (first 32 bytes)
- ASCII lines from device (e.g., debug logs)
- UUID strings for correlation
- CRC validation results

**Inspect raw packets**:
```python
uuid = dev.send_message(...)
print(f"Last response: {dev.last_response}")
# Keys: type, opcode, flags, uuid, payload, ts_unix, ts_ms, ts_tz, ts_flags
```

## See Also

- [../../../docs/BINARY_PROTOCOL.md](../../../docs/BINARY_PROTOCOL.md) — Protocol specification
- [../../../docs/COBS_ENCAPSULATION.md](../../../docs/COBS_ENCAPSULATION.md) — Framing details
- [example/example_python.py](example/example_python.py) — Complete CLI example
