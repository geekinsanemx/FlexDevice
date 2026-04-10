# AT Commands Reference
## FLEX-FSK-TX v2.5

All commands are terminated with `\r\n`. Responses end with `OK` or `ERROR`.

---

## Basic

| Command    | Description          | Response     |
|------------|----------------------|--------------|
| `AT`       | Connectivity check   | `OK`         |
| `AT+STATUS?` | Device status      | See below    |
| `AT+DEVICE?` | Full device info   | See below    |

### AT+STATUS?

```
+STATUS: IDLE,queue=0,bat=85%,freq=931.9375,power=10
OK
```

### AT+DEVICE?

```
+DEVICE: TTGO_FLEX_A1B2,v2.5.4,IDLE,931.9375MHz,10dBm,bat=85%
OK
```

---

## Radio Configuration

| Command              | Description                     | Example                  |
|----------------------|---------------------------------|--------------------------|
| `AT+FREQ=<MHz>`      | Set TX frequency                | `AT+FREQ=931.9375`       |
| `AT+FREQ?`           | Query frequency                 | `+FREQ: 931.9375`        |
| `AT+POWER=<dBm>`     | Set TX power (2-20)             | `AT+POWER=10`            |
| `AT+POWER?`          | Query TX power                  | `+POWER: 10`             |
| `AT+FREQPPM=<ppm>`   | Set frequency correction PPM    | `AT+FREQPPM=5`           |
| `AT+FREQPPM?`        | Query PPM correction            | `+FREQPPM: 5`            |

---

## Message Transmission

### AT+SEND=\<bytes\>

Send pre-encoded FLEX binary data (hex string).

```
AT+SEND=A1B2C3...
OK
```

### AT+MSG=\<capcode\>

Send FLEX message with text on the next line.

```
AT+MSG=1234567
Hello World
OK
```

### AT+MAILDROP=\<0|1\>

Set mail drop flag for next transmission.

```
AT+MAILDROP=1
OK
```

---

## WiFi / Network

| Command                    | Description                        |
|----------------------------|------------------------------------|
| `AT+WIFI=<ssid>,<pass>`    | Connect to WiFi network            |
| `AT+WIFI?`                 | Query WiFi status                  |
| `AT+WIFIENABLE=<0|1>`      | Disable/enable WiFi                |
| `AT+NETWORK=<mode>`        | Lock transport mode (GSM fw only)  |
| `AT+NETWORK?`              | Query current transport mode       |

Network modes (GSM firmware only): `AUTO`, `WIFI`, `GSM`, `AP`

---

## Device Configuration

| Command                  | Description                         |
|--------------------------|-------------------------------------|
| `AT+BANNER=<text>`       | Set custom display banner           |
| `AT+APIPORT=<port>`      | Set REST API port (default 80)      |
| `AT+SAVE`                | Save config to NVS                  |
| `AT+FACTORY`             | Factory reset (requires confirm)    |

---

## Clock

| Command                          | Description                        |
|----------------------------------|------------------------------------|
| `AT+CCLK=<unix_ts>,<tz_offset>`  | Set clock manually                 |
| `AT+CCLK?`                       | Query current clock                |

```
AT+CCLK=1775426200,-6.0
OK

AT+CCLK?
+CCLK: 1775426200,-6.0,2026-04-05 15:30:00
OK
```

Timezone offset in hours (float). `-6.0` = UTC-6.

---

## Logging

| Command         | Description                          |
|-----------------|--------------------------------------|
| `AT+LOGS?<N>`   | Query last N log lines (default 25)  |
| `AT+RMLOG`      | Delete persistent log file           |

---

## Battery

| Command       | Description         | Response example             |
|---------------|---------------------|------------------------------|
| `AT+BATTERY?` | Query battery info  | `+BATTERY: 85%,4150mV`       |

---

## Troubleshooting

**Device not responding:**
```bash
screen /dev/ttyUSB0 115200
AT
# Expect: OK
```

**Permission denied:**
```bash
sudo usermod -aG dialout $USER
# logout/login
```

**Check available ports:**
```bash
ls /dev/tty{USB,ACM}*
```

**Device detection:**
- Heltec WiFi LoRa 32 V2 → `/dev/ttyUSB0`
- TTGO LoRa32-OLED → `/dev/ttyACM0`
