# AT Commands Reference (v2.5.6)

All AT commands are line-based and terminated with `\r\n`.

- Success responses end with `OK`
- Invalid/failed operations return `ERROR`

## Basic

| Command | Description |
|---|---|
| `AT` | Connectivity check |
| `AT+STATUS?` | Current runtime state |
| `AT+DEVICE?` | Device/system information |
| `AT+RESET` | Reboot device |
| `AT+ABORT` | Abort current transmission state |

### `AT+STATUS?` example

```text
+STATUS: READY
OK
```

Possible states:
- `READY`
- `WAITING_DATA`
- `WAITING_MSG`
- `TRANSMITTING`
- `ERROR`

### `AT+DEVICE?` example

```text
+DEVICE_FIRMWARE: v2.5.6
+DEVICE_BATTERY: 85%
+DEVICE_MEMORY: 201376 bytes
+DEVICE_FLEX_CAPCODE: 1234567
+DEVICE_FLEX_FREQUENCY: 931.9375
+DEVICE_FLEX_POWER: 10.0
OK
```

## Radio Configuration

| Command | Description |
|---|---|
| `AT+FREQ=<MHz>` | Set TX frequency (`400.0..1000.0`) |
| `AT+FREQ?` | Query current TX frequency |
| `AT+FREQPPM=<ppm>` | Set frequency correction (`-50..+50`) |
| `AT+FREQPPM?` | Query frequency correction |
| `AT+POWER=<dBm>` | Set TX power (`2..20`) |
| `AT+POWER?` | Query TX power |

## Message Transmission

### `AT+SEND=<bytes>`

Puts device into binary-data receive mode for exactly `<bytes>` bytes.

```text
AT+SEND=128
+SEND: READY
...send 128 raw bytes...
OK
```

### `AT+MSG=<capcode>`

Puts device into message receive mode. Next line is message text.

```text
AT+MSG=1234567
+MSG: READY
Hello World
OK
```

### `AT+MAILDROP=<0|1>` / `AT+MAILDROP?`

Sets/queries maildrop behavior for next queued AT message.

## FLEX Defaults

| Command | Description |
|---|---|
| `AT+FLEX?` | Query default FLEX settings |
| `AT+FLEX=CAPCODE,<value>` | Set default capcode |
| `AT+FLEX=FREQUENCY,<value>` | Set default frequency |
| `AT+FLEX=POWER,<value>` | Set default TX power |

## Logging

| Command | Description |
|---|---|
| `AT+LOGS?<N>` | Print last N log lines (default 25) |
| `AT+RMLOG` | Delete log file |

## Time / Clock

| Command | Description |
|---|---|
| `AT+CCLK=<unix_ts>,<tz_hours>` | Set clock and optional timezone |
| `AT+CCLK?` | Query current clock |

Example:

```text
AT+CCLK=1775426200,-6.0
OK

AT+CCLK?
+CCLK: 1775426200,-6.0,2026-04-05 15:30:00
OK
```

## Factory Reset

| Command | Description |
|---|---|
| `AT+FACTORYRESET` | Run factory reset flow |

## Notes

- This firmware branch does not expose WiFi/network AT commands in `at_commands.cpp`.
- Binary protocol and AT share one UART; mode is selected by incoming stream content.

## Related Docs

- `docs/BINARY_PROTOCOL.md`
- `docs/COBS_ENCAPSULATION.md`
- `README.md`
