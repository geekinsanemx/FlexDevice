# COBS Encapsulation (Supplemental Reference)
## FLEX-FSK-TX Binary Protocol

> **Note**: This document provides extended COBS implementation details. For the complete binary protocol specification including packet structure, opcodes, and communication patterns, see [README.md](../README.md) or [BINARY_PROTOCOL.md](BINARY_PROTOCOL.md).

---

## What is COBS

COBS (Consistent Overhead Byte Stuffing) is the framing layer used for binary packets over UART. It eliminates all `0x00` bytes from the payload, allowing `0x00` to be used unambiguously as the frame delimiter.

Properties:
- Frame boundary is always `0x00` — no ambiguity, no escape sequences
- Overhead: at most 1 byte per 254 payload bytes (~0.4%)
- Resync: scan for `0x00` to find the next frame boundary

---

## Algorithm

### Encoding

Each run of non-zero bytes is prefixed with a length byte (how many bytes follow before the next zero, or end). Zero bytes are replaced with their distance overhead byte.

```
Input:  [0x11, 0x22, 0x00, 0x33]
           run=2       run=1
Output: [0x03, 0x11, 0x22, 0x02, 0x33, 0x00]
          ^len          ^len         ^delimiter
```

```
Input:  [0x11, 0x22, 0x33]     (no zeros)
Output: [0x04, 0x11, 0x22, 0x33, 0x00]
```

```
Input:  [0x00, 0x00, 0x00]     (all zeros)
Output: [0x01, 0x01, 0x01, 0x01, 0x00]
```

Special case: if a run reaches 254 bytes with no zero, overhead byte is `0xFF` and a new code block begins without inserting a zero.

### Decoding

Read the code byte. Copy `code-1` bytes verbatim. If `code < 0xFF`, insert a `0x00`. Repeat until `0x00` delimiter.

---

## Implementation

### C (from FlexDevice.h)

```c
size_t cobs_encode(const uint8_t *input, size_t length, uint8_t *output) {
    const uint8_t *src = input;
    uint8_t *dst = output;
    uint8_t *code_ptr = dst++;
    uint8_t code = 0x01;

    for (size_t i = 0; i < length; i++) {
        if (*src == 0x00) {
            *code_ptr = code;
            code_ptr = dst++;
            code = 0x01;
        } else {
            *dst++ = *src;
            code++;
            if (code == 0xFF) {
                *code_ptr = code;
                code_ptr = dst++;
                code = 0x01;
            }
        }
        src++;
    }
    *code_ptr = code;
    *dst++ = 0x00;
    return (size_t)(dst - output);
}

size_t cobs_decode(const uint8_t *input, size_t length, uint8_t *output) {
    if (length == 0 || input[length - 1] != 0x00) return 0;
    const uint8_t *src = input;
    uint8_t *dst = output;
    size_t remaining = length - 1;

    while (remaining > 0) {
        uint8_t code = *src++;
        remaining--;
        if (code == 0x00) return 0;  // Invalid
        for (uint8_t i = 1; i < code && remaining > 0; i++) {
            *dst++ = *src++;
            remaining--;
        }
        if (code < 0xFF && remaining > 0) {
            *dst++ = 0x00;
        }
    }
    return (size_t)(dst - output);
}
```

### Python (from FlexDevice.py)

```python
def cobs_encode(data: bytes) -> bytes:
    output = bytearray()
    code_ptr = 0
    code = 0x01
    output.append(0)

    for byte in data:
        if byte == 0x00:
            output[code_ptr] = code
            code_ptr = len(output)
            output.append(0)
            code = 0x01
        else:
            output.append(byte)
            code += 1
            if code == 0xFF:
                output[code_ptr] = code
                code_ptr = len(output)
                output.append(0)
                code = 0x01

    output[code_ptr] = code
    output.append(0x00)
    return bytes(output)


def cobs_decode(data: bytes) -> bytes:
    if not data or data[-1] != 0x00:
        raise ValueError("Invalid COBS frame")

    output = bytearray()
    src = data[:-1]
    idx = 0

    while idx < len(src):
        code = src[idx]
        if code == 0x00:
            raise ValueError("Unexpected zero in COBS data")
        idx += 1
        for _ in range(1, code):
            output.append(src[idx])
            idx += 1
        if code < 0xFF and idx < len(src):
            output.append(0x00)

    return bytes(output)
```

---

## Packet on the Wire

A 512-byte binary packet after COBS encoding becomes at most 514 bytes (512 + 1 overhead byte + 1 delimiter). In practice for FLEX packets the encoded size is 513-514 bytes.

```
[ COBS_ENCODED(512 bytes) ] [ 0x00 ]
  513-514 bytes               1 byte
```

---

## Framing Rules

1. **Transmit:** encode the full 512-byte packet, write the COBS output (which ends with `0x00`) to the UART.
2. **Receive:** accumulate bytes until `0x00` is read. That completes one frame. Pass the frame (including the `0x00`) to `cobs_decode`.
3. **Mixed ASCII/Binary:** the firmware may send ASCII log lines (printable bytes + `\n`) before or after binary frames. A valid COBS frame never contains printable-only sequences — the first non-printable, non-newline byte signals the start of a binary frame.

---

## Test Vectors

```
Input:  11 22 00 33
Encoded: 03 11 22 02 33 00

Input:  11 22 33
Encoded: 04 11 22 33 00

Input:  00 00 00
Encoded: 01 01 01 01 00

Input:  FF (254 non-zero bytes)
Encoded: FF [254 bytes] 01 00
```

---

## CRC16-CCITT

The CRC covers the raw 512-byte packet before COBS encoding (bytes 0-509). It is stored at bytes 510-511 as a native `uint16_t`.

- Polynomial: `0x1021`
- Initial value: `0xFFFF`
- No reflection
- Test vector: `"123456789"` → `0x29B1`

```c
uint16_t crc16_ccitt(const uint8_t *data, size_t len) {
    uint16_t crc = 0xFFFF;
    for (size_t i = 0; i < len; i++) {
        crc = (uint16_t)((crc << 8) ^ table[((crc >> 8) ^ data[i]) & 0xFF]);
    }
    return crc;
}
```
