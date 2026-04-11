# Binary Protocol Tools

User-space clients for the FLEX-FSK-TX v2.5.6 binary UART protocol. Everything
under `cli/` targets Linux (or any POSIX) and requires only `gcc` and `pyserial`
for the Python pieces.

```
cli/
├── flex-cli.c                 # Stand-alone CLI source
├── flex-cli                   # Prebuilt ELF (optional)
├── libflex_binary/            # C header-only library + GCC example
│   ├── FlexDevice.h
│   ├── example_gcc.c
│   └── example_gcc
└── libraries/python/          # Python library and diagnostics helper
    ├── FlexDevice.py
    └── example/diagnose_flex_frames.py
```

---

## C Library Quick Start

```c
#include "FlexDevice.h"

FlexDevice dev;
flex_open(&dev, "/dev/ttyUSB0", 115200);
flex_reset_lines(&dev, 100);  // optional: pulse DTR/RTS to resync

char uuid[37];
flex_send_msg(&dev, 1234567, 931.9375, 10, 0, "Hello World", uuid);
printf("UUID: %s\n", uuid);

flex_close(&dev);
```

```bash
gcc -o myapp myapp.c -O2 -Wall
```

**API summary**

```c
int  flex_open(FlexDevice *dev, const char *device, int baudrate);
void flex_close(FlexDevice *dev);
void flex_reset_lines(FlexDevice *dev, int delay_ms); // optional handshake

int  flex_send_msg(FlexDevice *dev, uint64_t capcode, float frequency,
                   int8_t power, uint8_t mail_drop, const char *message,
                   char *uuid_out);  // uuid_out: 37-byte buffer or NULL

int  flex_send_msg_wait(FlexDevice *dev, uint64_t capcode, float frequency,
                        int8_t power, uint8_t mail_drop, const char *message,
                        char *uuid_out, int wait_timeout_sec);

int  flex_ping(FlexDevice *dev);

int  flex_get_status(FlexDevice *dev, uint8_t *device_state,
                     uint8_t *queue_count, uint8_t *battery_pct,
                     uint16_t *battery_mv, float *frequency, int8_t *power);
```

Set `dev.verbose = 1` for debug output (hex dumps of every TX/RX frame).

---

## Python Library Quick Start

```python
from FlexDevice import FlexDevice

with FlexDevice('/dev/ttyUSB0', verbose=True) as dev:
    uuid = dev.send_message_wait(
        capcode=1234567,
        frequency=931.9375,
        power=10,
        message="Hello World",
        wait_timeout=30.0,
    )
    print(f"UUID: {uuid}")
```

```bash
pip3 install pyserial
```

**API summary**

```python
dev = FlexDevice(port, baudrate=115200, timeout=5.0, verbose=False)
dev.open()                # or use context manager
uuid = dev.send_message(...)
uuid = dev.send_message_wait(..., wait_timeout=30.0)
rtt_ms = dev.ping()
status = dev.get_status()
```

`dev.last_response` stores the last parsed packet (dict) for inspection.

---

### Diagnostics Helper

```
python3 libraries/python/example/diagnose_flex_frames.py \
  --port /dev/ttyUSB0 --capcode 37137 --frequency 931.9375 \
  --power 10 --message "Hello FLEX" --wait 40 --dump-dir ./frames --verbose
```

The script logs every raw COBS frame, checks the decoded length
(`expected 512`), and optionally dumps frames to disk. Use `--reset-lines`
if your USB‑serial adapter needs DTR/RTS pulsing before the first transfer.

---

## Stand-alone CLI (`flex-cli`)

```bash
gcc -o flex-cli flex-cli.c -O2 -Wall
./flex-cli -d /dev/ttyUSB0 -f 931.9375 -p 10 -w -R 1234567 "Hello World"
```

Options:

- `-d DEVICE` – serial port (default `/dev/ttyUSB0`)
- `-b BAUD` – baudrate (default `115200`)
- `-f FREQ` – frequency in MHz (default `931.9375`)
- `-p POWER` – TX power in dBm (default `10`)
- `-m` – enable mail drop flag
- `-v` – verbose output (hex dumps for TX/RX)
- `-w` – wait for `TX_DONE`/`TX_FAILED` (30 s timeout)
- `-s` – query device status before sending
- `-P` – ping device before sending
- `-R` – pulse DTR/RTS before transmitting (helps resync some adapters)

---

## Protocol Docs

- [docs/BINARY_PROTOCOL.md](../docs/BINARY_PROTOCOL.md) — Packet structure,
  opcodes, payload layouts, full protocol spec
- [docs/COBS_ENCAPSULATION.md](../docs/COBS_ENCAPSULATION.md) — Framing,
  encoding algorithm, test vectors
- [docs/AT_COMMANDS.md](../docs/AT_COMMANDS.md) — AT command reference

---

## Troubleshooting

**Permission denied**

```bash
sudo usermod -aG dialout $USER
# log out / back in
```

**Timeout waiting for ACK**

- Confirm firmware `v2.5.6` (older versions logged ASCII during binary sessions)
- Verify baudrate (default `115200`)
- Try a different USB cable or port
- Pulse DTR/RTS (`flex_reset_lines` or CLI `-R`) to re-sync UART adapters

**Device ports**

- Heltec WiFi LoRa 32 V2 → `/dev/ttyUSB0`
- TTGO LoRa32-OLED       → `/dev/ttyACM0`
