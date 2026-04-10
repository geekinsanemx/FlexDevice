# Binary Protocol Tools

Tools for FLEX-FSK-TX v2.5 binary UART protocol.

## Structure

```
tools/
├── libflex_binary/       # C header-only library
│   ├── FlexDevice.h      # Single-header library (include this)
│   ├── example.c         # C usage example
│   └── README.md
├── python/               # Python library
│   ├── FlexDevice.py     # Module (copy to your project)
│   ├── example_python.py # Python usage example
│   └── README.md
├── flex-binary-client.c  # Standalone CLI tool
└── test-edge-cases.sh    # Edge case test script
```

---

## C Library Quick Start

```c
#include "FlexDevice.h"

FlexDevice dev;
flex_open(&dev, "/dev/ttyUSB0", 115200);

char uuid[37];
flex_send_msg(&dev, 1234567, 931.9375, 10, 0, "Hello World", uuid);
printf("UUID: %s\n", uuid);

flex_close(&dev);
```

```bash
gcc -o myapp myapp.c -O2 -Wall
```

**API:**

```c
int  flex_open(FlexDevice *dev, const char *device, int baudrate);
void flex_close(FlexDevice *dev);

// Send and wait for ACK
int  flex_send_msg(FlexDevice *dev, uint32_t capcode, float frequency,
                   int8_t power, uint8_t mail_drop, const char *message,
                   char *uuid_out);  // uuid_out: 37-byte buffer or NULL

// Send and wait for ACK + TX_DONE event
int  flex_send_msg_wait(FlexDevice *dev, uint32_t capcode, float frequency,
                        int8_t power, uint8_t mail_drop, const char *message,
                        char *uuid_out, int wait_timeout_sec);

int  flex_ping(FlexDevice *dev);

int  flex_get_status(FlexDevice *dev, uint8_t *device_state,
                     uint8_t *queue_count, uint8_t *battery_pct,
                     uint16_t *battery_mv, float *frequency, int8_t *power);
```

Set `dev.verbose = 1` for debug output.

---

## Python Library Quick Start

```python
from FlexDevice import FlexDevice

with FlexDevice('/dev/ttyUSB0') as dev:
    uuid = dev.send_message(capcode=1234567, frequency=931.9375,
                            power=10, message="Hello World")
    print(f"UUID: {uuid}")
```

```bash
pip3 install pyserial
```

**API:**

```python
dev = FlexDevice(port, baudrate=115200, timeout=5.0, verbose=False)
dev.open() / dev.close()  # or use as context manager

uuid: str = dev.send_message(capcode, frequency, power, message, mail_drop=False)
uuid: str = dev.send_message_wait(capcode, frequency, power, message,
                                   mail_drop=False, wait_timeout=30.0)
rtt_ms: float = dev.ping()
status: dict  = dev.get_status()
```

`dev.last_response` holds the last parsed response dict after any call.

---

## Standalone CLI Client

```bash
gcc -o flex-binary-client flex-binary-client.c -O2 -Wall
./flex-binary-client -d /dev/ttyUSB0 -f 931.9375 -p 10 -w 1234567 "Hello World"
```

Options: `-d DEVICE`, `-b BAUD`, `-f FREQ`, `-p POWER`, `-m` (mail drop), `-v` (verbose), `-w` (wait TX_DONE), `-s` (get status), `-P` (ping)

---

## Protocol Docs

- [docs/BINARY_PROTOCOL.md](../docs/BINARY_PROTOCOL.md) — Packet structure, opcodes, payload layouts, full protocol spec
- [docs/COBS_ENCAPSULATION.md](../docs/COBS_ENCAPSULATION.md) — Framing, encoding algorithm, test vectors
- [docs/AT_COMMANDS.md](../docs/AT_COMMANDS.md) — AT command reference

---

## Troubleshooting

**Permission denied:**
```bash
sudo usermod -aG dialout $USER
# logout/login
```

**Timeout waiting for ACK:**
- Verify firmware v2.5.1+ on the ESP32
- Verify baudrate (default 115200)
- Try a different USB cable

**Device ports:**
- Heltec WiFi LoRa 32 V2 → `/dev/ttyUSB0`
- TTGO LoRa32-OLED → `/dev/ttyACM0`
