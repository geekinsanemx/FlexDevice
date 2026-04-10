#!/usr/bin/env python3
"""
FlexDevice.py - FLEX Binary Protocol Client Library

Python module for communicating with FLEX-FSK-TX firmware via binary
COBS-framed protocol over UART.

Usage:
    from FlexDevice import FlexDevice

    with FlexDevice('/dev/ttyUSB0') as dev:
        uuid = dev.send_message(capcode=37137, frequency=931.9375,
                                power=10, message="Hello World")
        print(f"Sent: {uuid}")

Protocol reference: binary_packet.h in firmware source
"""

import struct
import uuid as uuid_module
import time
import serial
from datetime import datetime, timezone

__version__ = '2.5.0'

# =============================================================================
# PROTOCOL CONSTANTS  (must match binary_packet.h in firmware)
# =============================================================================

PACKET_FIXED_SIZE    = 512
PACKET_PAYLOAD_SIZE  = 480
PACKET_CRC_OFFSET    = 510
PACKET_TS_OFFSET     = 502
MAX_MESSAGE_PROTO    = 255
CMD_SEND_ARGS_SIZE   = 11

PKT_TYPE_CMD = 0x01
PKT_TYPE_RSP = 0x02
PKT_TYPE_EVT = 0x03

CMD_SEND_FLEX  = 0x01
CMD_GET_STATUS = 0x02
CMD_PING       = 0x06

RSP_ACK    = 0x01
RSP_NACK   = 0x02
RSP_STATUS = 0x03
RSP_PONG   = 0x05

EVT_TX_QUEUED  = 0x01
EVT_TX_START   = 0x02
EVT_TX_DONE    = 0x03
EVT_TX_FAILED  = 0x04

FLAG_ACK_REQUIRED = 0x01

STATUS_ACCEPTED      = 0x00
STATUS_REJECTED      = 0x01
STATUS_QUEUE_FULL    = 0x02
STATUS_INVALID_PARAM = 0x03
STATUS_BUSY          = 0x04
STATUS_ERROR         = 0x05

RESULT_SUCCESS        = 0x00
RESULT_RADIO_ERROR    = 0x01
RESULT_ENCODING_ERROR = 0x02
RESULT_TIMEOUT        = 0x03
RESULT_ABORTED        = 0x04

TS_FLAG_VALID       = 0x01
TS_FLAG_AUTO_ADJUST = 0x02
TS_FLAG_SYNC_RTC    = 0x04

# =============================================================================
# EXCEPTIONS
# =============================================================================

class FlexError(Exception):
    pass

class FlexTimeoutError(FlexError):
    pass

class FlexNackError(FlexError):
    def __init__(self, status):
        self.status = status
        super().__init__(f"NACK status=0x{status:02X}")

class FlexRejectedError(FlexError):
    def __init__(self, status):
        self.status = status
        super().__init__(f"ACK rejected status=0x{status:02X}")

class FlexTxFailedError(FlexError):
    def __init__(self, result):
        self.result = result
        super().__init__(f"TX_FAILED result=0x{result:02X}")

# =============================================================================
# CRC16-CCITT
# =============================================================================

_CRC16_TABLE = [
    0x0000, 0x1021, 0x2042, 0x3063, 0x4084, 0x50A5, 0x60C6, 0x70E7,
    0x8108, 0x9129, 0xA14A, 0xB16B, 0xC18C, 0xD1AD, 0xE1CE, 0xF1EF,
    0x1231, 0x0210, 0x3273, 0x2252, 0x52B5, 0x4294, 0x72F7, 0x62D6,
    0x9339, 0x8318, 0xB37B, 0xA35A, 0xD3BD, 0xC39C, 0xF3FF, 0xE3DE,
    0x2462, 0x3443, 0x0420, 0x1401, 0x64E6, 0x74C7, 0x44A4, 0x5485,
    0xA56A, 0xB54B, 0x8528, 0x9509, 0xE5EE, 0xF5CF, 0xC5AC, 0xD58D,
    0x3653, 0x2672, 0x1611, 0x0630, 0x76D7, 0x66F6, 0x5695, 0x46B4,
    0xB75B, 0xA77A, 0x9719, 0x8738, 0xF7DF, 0xE7FE, 0xD79D, 0xC7BC,
    0x48C4, 0x58E5, 0x6886, 0x78A7, 0x0840, 0x1861, 0x2802, 0x3823,
    0xC9CC, 0xD9ED, 0xE98E, 0xF9AF, 0x8948, 0x9969, 0xA90A, 0xB92B,
    0x5AF5, 0x4AD4, 0x7AB7, 0x6A96, 0x1A71, 0x0A50, 0x3A33, 0x2A12,
    0xDBFD, 0xCBDC, 0xFBBF, 0xEB9E, 0x9B79, 0x8B58, 0xBB3B, 0xAB1A,
    0x6CA6, 0x7C87, 0x4CE4, 0x5CC5, 0x2C22, 0x3C03, 0x0C60, 0x1C41,
    0xEDAE, 0xFD8F, 0xCDEC, 0xDDCD, 0xAD2A, 0xBD0B, 0x8D68, 0x9D49,
    0x7E97, 0x6EB6, 0x5ED5, 0x4EF4, 0x3E13, 0x2E32, 0x1E51, 0x0E70,
    0xFF9F, 0xEFBE, 0xDFDD, 0xCFFC, 0xBF1B, 0xAF3A, 0x9F59, 0x8F78,
    0x9188, 0x81A9, 0xB1CA, 0xA1EB, 0xD10C, 0xC12D, 0xF14E, 0xE16F,
    0x1080, 0x00A1, 0x30C2, 0x20E3, 0x5004, 0x4025, 0x7046, 0x6067,
    0x83B9, 0x9398, 0xA3FB, 0xB3DA, 0xC33D, 0xD31C, 0xE37F, 0xF35E,
    0x02B1, 0x1290, 0x22F3, 0x32D2, 0x4235, 0x5214, 0x6277, 0x7256,
    0xB5EA, 0xA5CB, 0x95A8, 0x8589, 0xF56E, 0xE54F, 0xD52C, 0xC50D,
    0x34E2, 0x24C3, 0x14A0, 0x0481, 0x7466, 0x6447, 0x5424, 0x4405,
    0xA7DB, 0xB7FA, 0x8799, 0x97B8, 0xE75F, 0xF77E, 0xC71D, 0xD73C,
    0x26D3, 0x36F2, 0x0691, 0x16B0, 0x6657, 0x7676, 0x4615, 0x5634,
    0xD94C, 0xC96D, 0xF90E, 0xE92F, 0x99C8, 0x89E9, 0xB98A, 0xA9AB,
    0x5844, 0x4865, 0x7806, 0x6827, 0x18C0, 0x08E1, 0x3882, 0x28A3,
    0xCB7D, 0xDB5C, 0xEB3F, 0xFB1E, 0x8BF9, 0x9BD8, 0xABBB, 0xBB9A,
    0x4A75, 0x5A54, 0x6A37, 0x7A16, 0x0AF1, 0x1AD0, 0x2AB3, 0x3A92,
    0xFD2E, 0xED0F, 0xDD6C, 0xCD4D, 0xBDAA, 0xAD8B, 0x9DE8, 0x8DC9,
    0x7C26, 0x6C07, 0x5C64, 0x4C45, 0x3CA2, 0x2C83, 0x1CE0, 0x0CC1,
    0xEF1F, 0xFF3E, 0xCF5D, 0xDF7C, 0xAF9B, 0xBFBA, 0x8FD9, 0x9FF8,
    0x6E17, 0x7E36, 0x4E55, 0x5E74, 0x2E93, 0x3EB2, 0x0ED1, 0x1EF0,
]


def _crc16(data: bytes) -> int:
    crc = 0xFFFF
    for b in data:
        crc = ((crc << 8) & 0xFFFF) ^ _CRC16_TABLE[((crc >> 8) ^ b) & 0xFF]
    return crc


# =============================================================================
# COBS
# =============================================================================

def _cobs_encode(data: bytes) -> bytes:
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


def _cobs_decode(data: bytes) -> bytes:
    if not data or data[-1] != 0x00:
        raise FlexError("Invalid COBS frame: missing delimiter")

    output = bytearray()
    src = data[:-1]
    idx = 0

    while idx < len(src):
        code = src[idx]
        if code == 0x00:
            raise FlexError("Unexpected zero byte in COBS data")
        idx += 1
        for _ in range(1, code):
            if idx >= len(src):
                break
            output.append(src[idx])
            idx += 1
        if code < 0xFF and idx < len(src):
            output.append(0x00)

    return bytes(output)


# =============================================================================
# PACKET BUILDER
# =============================================================================

def _build_packet(pkt_type: int, opcode: int, seq: int,
                  msg_uuid: bytes, payload: bytes) -> bytes:
    raw = bytearray(PACKET_FIXED_SIZE)

    raw[0] = pkt_type
    raw[1] = opcode
    raw[2] = FLAG_ACK_REQUIRED
    raw[3] = seq & 0xFF
    raw[4:20] = msg_uuid[:16]

    raw[20:22] = struct.pack('>H', len(payload))

    if payload:
        raw[22:22 + len(payload)] = payload

    # Timestamp at offset 502-509 (big-endian per firmware spec)
    now = datetime.now(timezone.utc)
    unix_ts = int(now.timestamp())
    millis  = int((now.timestamp() % 1) * 1000)
    local_offset = time.timezone if not time.daylight else time.altzone
    tz_units = -int(local_offset / 1800)

    raw[PACKET_TS_OFFSET:PACKET_TS_OFFSET + 4] = struct.pack('>I', unix_ts)
    raw[PACKET_TS_OFFSET + 4:PACKET_TS_OFFSET + 6] = struct.pack('>H', millis)
    raw[PACKET_TS_OFFSET + 6] = tz_units & 0xFF
    raw[PACKET_TS_OFFSET + 7] = TS_FLAG_VALID | TS_FLAG_AUTO_ADJUST | TS_FLAG_SYNC_RTC

    # CRC over first 510 bytes, stored as native uint16 (same as firmware: pkt->crc16 = crc)
    crc = _crc16(bytes(raw[:PACKET_CRC_OFFSET]))
    struct.pack_into('<H', raw, PACKET_CRC_OFFSET, crc)

    return bytes(raw)


def _build_cmd_send_flex(seq: int, msg_uuid: bytes, capcode: int, frequency: float,
                         power: int, mail_drop: int, message: bytes) -> bytes:
    payload = bytearray()
    payload += struct.pack('<I', capcode)       # little-endian uint32
    payload += struct.pack('<f', frequency)     # IEEE 754 float, little-endian
    payload += struct.pack('b', power)          # signed int8
    payload += struct.pack('B', mail_drop)      # uint8
    payload += struct.pack('B', len(message))   # uint8 length
    payload += message
    return _build_packet(PKT_TYPE_CMD, CMD_SEND_FLEX, seq, msg_uuid, bytes(payload))


def _build_cmd_ping(seq: int, msg_uuid: bytes) -> bytes:
    return _build_packet(PKT_TYPE_CMD, CMD_PING, seq, msg_uuid, b'')


def _build_cmd_get_status(seq: int, msg_uuid: bytes) -> bytes:
    return _build_packet(PKT_TYPE_CMD, CMD_GET_STATUS, seq, msg_uuid, b'')


# =============================================================================
# PACKET PARSER
# =============================================================================

def _parse_packet(data: bytes) -> dict:
    if len(data) != PACKET_FIXED_SIZE:
        raise FlexError(f"Invalid decoded size {len(data)} (expected {PACKET_FIXED_SIZE})")

    pkt_crc, = struct.unpack_from('<H', data, PACKET_CRC_OFFSET)
    calc_crc  = _crc16(data[:PACKET_CRC_OFFSET])
    if pkt_crc != calc_crc:
        raise FlexError(f"CRC mismatch: got 0x{pkt_crc:04X} expected 0x{calc_crc:04X}")

    pkt_type  = data[0]
    opcode    = data[1]
    flags     = data[2]
    seq       = data[3]
    pkt_uuid  = data[4:20]
    payload_len, = struct.unpack_from('>H', data, 20)
    payload   = data[22:22 + payload_len] if payload_len <= PACKET_PAYLOAD_SIZE else data[22:22 + PACKET_PAYLOAD_SIZE]

    ts_unix, = struct.unpack_from('>I', data, PACKET_TS_OFFSET)
    ts_ms,   = struct.unpack_from('>H', data, PACKET_TS_OFFSET + 4)
    ts_tz    = struct.unpack_from('b',  data, PACKET_TS_OFFSET + 6)[0]
    ts_flags = data[PACKET_TS_OFFSET + 7]

    return {
        'type':     pkt_type,
        'opcode':   opcode,
        'flags':    flags,
        'seq':      seq,
        'uuid':     pkt_uuid,
        'payload':  payload,
        'ts_unix':  ts_unix,
        'ts_ms':    ts_ms,
        'ts_tz':    ts_tz,
        'ts_flags': ts_flags,
    }


# =============================================================================
# FlexDevice CLASS
# =============================================================================

class FlexDevice:
    """FLEX binary protocol device interface."""

    def __init__(self, port: str, baudrate: int = 115200,
                 timeout: float = 5.0, verbose: bool = False):
        self.port     = port
        self.baudrate = baudrate
        self.timeout  = timeout
        self.verbose  = verbose
        self._serial  = None
        self._seq     = 1
        self.last_response = None  # last parsed response dict

    def __enter__(self):
        self.open()
        return self

    def __exit__(self, *_):
        self.close()

    def open(self):
        try:
            self._serial = serial.Serial(
                port=self.port,
                baudrate=self.baudrate,
                bytesize=serial.EIGHTBITS,
                parity=serial.PARITY_NONE,
                stopbits=serial.STOPBITS_ONE,
                timeout=0.05,
                write_timeout=2.0,
            )
            self._serial.reset_input_buffer()
            self._serial.reset_output_buffer()
        except serial.SerialException as e:
            raise FlexError(f"Failed to open {self.port}: {e}")

    def close(self):
        if self._serial and self._serial.is_open:
            self._serial.close()

    # --------------------------------------------------------------------------
    # PUBLIC API
    # --------------------------------------------------------------------------

    def send_message(self, capcode: int, frequency: float, power: int,
                     message: str, mail_drop: bool = False) -> str:
        """
        Send FLEX message and wait for ACK.

        Returns:
            UUID string of the sent message.

        Raises:
            FlexTimeoutError, FlexNackError, FlexRejectedError, FlexError
        """
        msg_uuid  = uuid_module.uuid4().bytes
        msg_bytes = message.encode('utf-8')[:MAX_MESSAGE_PROTO]

        raw = _build_cmd_send_flex(self._seq, msg_uuid, capcode, frequency,
                                    power, int(mail_drop), msg_bytes)
        self._seq = (self._seq + 1) & 0xFF

        self._send_raw(raw)

        rsp = self._recv_packet(timeout_ms=int(self.timeout * 1000))
        self.last_response = rsp

        if rsp['type'] == PKT_TYPE_RSP and rsp['opcode'] == RSP_ACK:
            status = rsp['payload'][0] if rsp['payload'] else 0xFF
            if status != STATUS_ACCEPTED:
                raise FlexRejectedError(status)
            return str(uuid_module.UUID(bytes=msg_uuid))

        if rsp['type'] == PKT_TYPE_RSP and rsp['opcode'] == RSP_NACK:
            raise FlexNackError(rsp['payload'][0] if rsp['payload'] else 0xFF)

        raise FlexError(f"Unexpected response type=0x{rsp['type']:02X} opcode=0x{rsp['opcode']:02X}")

    def send_message_wait(self, capcode: int, frequency: float, power: int,
                          message: str, mail_drop: bool = False,
                          wait_timeout: float = 30.0) -> str:
        """
        Send FLEX message, wait for ACK, then wait for TX_DONE/TX_FAILED events.

        Returns:
            UUID string on TX_DONE SUCCESS.

        Raises:
            FlexTimeoutError, FlexNackError, FlexRejectedError, FlexTxFailedError, FlexError
        """
        msg_uuid  = uuid_module.uuid4().bytes
        msg_bytes = message.encode('utf-8')[:MAX_MESSAGE_PROTO]

        raw = _build_cmd_send_flex(self._seq, msg_uuid, capcode, frequency,
                                    power, int(mail_drop), msg_bytes)
        self._seq = (self._seq + 1) & 0xFF

        self._send_raw(raw)

        # Wait for ACK
        rsp = self._recv_packet(timeout_ms=int(self.timeout * 1000))
        self.last_response = rsp

        if rsp['type'] == PKT_TYPE_RSP and rsp['opcode'] == RSP_NACK:
            raise FlexNackError(rsp['payload'][0] if rsp['payload'] else 0xFF)

        if not (rsp['type'] == PKT_TYPE_RSP and rsp['opcode'] == RSP_ACK):
            raise FlexError(f"Unexpected ACK response type=0x{rsp['type']:02X} opcode=0x{rsp['opcode']:02X}")

        status = rsp['payload'][0] if rsp['payload'] else 0xFF
        if status != STATUS_ACCEPTED:
            raise FlexRejectedError(status)

        uuid_str = str(uuid_module.UUID(bytes=msg_uuid))

        # Wait for TX_DONE/TX_FAILED
        deadline = time.time() + wait_timeout
        while time.time() < deadline:
            remaining_ms = int((deadline - time.time()) * 1000)
            if remaining_ms <= 0:
                break
            try:
                evt = self._recv_packet(timeout_ms=min(remaining_ms, 1000))
            except FlexTimeoutError:
                continue

            self.last_response = evt
            if evt['type'] != PKT_TYPE_EVT:
                continue

            opcode      = evt['opcode']
            evt_uuid    = evt['uuid']
            uuid_match  = (evt_uuid == msg_uuid)
            evt_uuid_str = str(uuid_module.UUID(bytes=evt_uuid))

            if opcode == EVT_TX_QUEUED:
                if self.verbose:
                    pos = evt['payload'][0] if evt['payload'] else 0
                    print(f"EVENT: TX_QUEUED pos={pos} (uuid={evt_uuid_str})")

            elif opcode == EVT_TX_START:
                print(f"EVENT: TX_START (uuid={evt_uuid_str})")

            elif opcode == EVT_TX_DONE:
                if uuid_match:
                    result = evt['payload'][0] if evt['payload'] else 0xFF
                    if result == RESULT_SUCCESS:
                        return uuid_str
                    raise FlexTxFailedError(result)
                elif self.verbose:
                    print(f"EVENT: TX_DONE for other message (uuid={evt_uuid_str})")

            elif opcode == EVT_TX_FAILED:
                if uuid_match:
                    result = evt['payload'][0] if evt['payload'] else 0xFF
                    raise FlexTxFailedError(result)
                elif self.verbose:
                    print(f"EVENT: TX_FAILED for other message (uuid={evt_uuid_str})")

            elif self.verbose:
                print(f"EVENT: opcode=0x{opcode:02X} (uuid={evt_uuid_str})")

        raise FlexTimeoutError("Timeout waiting for TX_DONE")

    def ping(self) -> float:
        """Send PING and return round-trip time in milliseconds."""
        msg_uuid = uuid_module.uuid4().bytes
        raw = _build_cmd_ping(self._seq, msg_uuid)
        self._seq = (self._seq + 1) & 0xFF

        t0 = time.time()
        self._send_raw(raw)
        rsp = self._recv_packet(timeout_ms=3000)
        rtt = (time.time() - t0) * 1000

        if rsp['type'] == PKT_TYPE_RSP and rsp['opcode'] == RSP_PONG:
            return rtt
        raise FlexError(f"Unexpected ping response type=0x{rsp['type']:02X} opcode=0x{rsp['opcode']:02X}")

    def get_status(self) -> dict:
        """
        Query device status.

        Returns dict with keys:
          device_state, queue_count, battery_pct, battery_mv, frequency, power
        """
        msg_uuid = uuid_module.uuid4().bytes
        raw = _build_cmd_get_status(self._seq, msg_uuid)
        self._seq = (self._seq + 1) & 0xFF

        self._send_raw(raw)
        rsp = self._recv_packet(timeout_ms=3000)
        self.last_response = rsp

        if not (rsp['type'] == PKT_TYPE_RSP and rsp['opcode'] == RSP_STATUS):
            raise FlexError(f"Unexpected status response type=0x{rsp['type']:02X} opcode=0x{rsp['opcode']:02X}")

        p = rsp['payload']
        # RSP_STATUS payload layout from rsp_status_payload_t:
        # [0]   device_state (uint8)
        # [1]   queue_count  (uint8)
        # [2]   battery_pct  (uint8)
        # [3-4] battery_mv   (uint16 little-endian)
        # [5-8] frequency    (float little-endian)
        # [9]   power        (int8)
        device_state, = struct.unpack_from('B', p, 0)
        queue_count,  = struct.unpack_from('B', p, 1)
        battery_pct,  = struct.unpack_from('B', p, 2)
        battery_mv,   = struct.unpack_from('<H', p, 3)
        frequency,    = struct.unpack_from('<f', p, 5)
        power,        = struct.unpack_from('b', p, 9)

        return {
            'device_state': device_state,
            'queue_count':  queue_count,
            'battery_pct':  battery_pct,
            'battery_mv':   battery_mv,
            'frequency':    frequency,
            'power':        power,
        }

    # --------------------------------------------------------------------------
    # INTERNAL
    # --------------------------------------------------------------------------

    def _send_raw(self, raw: bytes):
        cobs_data = _cobs_encode(raw)
        if self.verbose:
            uuid_str = str(uuid_module.UUID(bytes=raw[4:20]))
            print(f"TX [uuid={uuid_str}, {len(cobs_data)} COBS bytes]")
        self._serial.write(cobs_data)
        self._serial.flush()

    def _read_frame(self, timeout_ms: int) -> bytes:
        """
        Read one COBS frame (up to and including 0x00 delimiter).
        Drains and prints ASCII lines from device; returns binary frame.
        """
        frame     = bytearray()
        ascii_buf = bytearray()
        in_binary = False
        deadline  = time.time() + timeout_ms / 1000.0

        while True:
            if time.time() > deadline:
                raise FlexTimeoutError("Timeout waiting for response frame")

            b = self._serial.read(1)
            if not b:
                continue

            byte = b[0]

            if not in_binary:
                if byte == ord('\n'):
                    line = ascii_buf.decode('ascii', errors='replace').strip()
                    if line:
                        print(f"DEVICE: {line}")
                    ascii_buf.clear()
                    continue
                if byte == ord('\r'):
                    continue
                if 0x20 <= byte <= 0x7E:
                    ascii_buf.append(byte)
                    continue
                in_binary = True

            frame.append(byte)
            if byte == 0x00:
                return bytes(frame)

    def _recv_packet(self, timeout_ms: int) -> dict:
        frame   = self._read_frame(timeout_ms)
        decoded = _cobs_decode(frame)
        pkt     = _parse_packet(decoded)

        if self.verbose:
            uuid_str = str(uuid_module.UUID(bytes=pkt['uuid']))
            print(f"PARSED: type=0x{pkt['type']:02X} opcode=0x{pkt['opcode']:02X} "
                  f"seq={pkt['seq']} uuid={uuid_str}")
            if pkt['ts_flags'] & TS_FLAG_VALID:
                ts = datetime.fromtimestamp(pkt['ts_unix']).strftime('%Y-%m-%d %H:%M:%S')
                print(f"ESP32 time: {ts}.{pkt['ts_ms']:03d} (UTC{pkt['ts_tz'] * 0.5:+.1f})")

        return pkt
