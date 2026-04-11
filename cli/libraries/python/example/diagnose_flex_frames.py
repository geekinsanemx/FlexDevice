#!/usr/bin/env python3
from __future__ import annotations

"""
diagnose_flex_frames.py - Binary protocol diagnostics helper

This script uses the FlexDevice client library to send a FLEX message and
records every binary frame coming back from the device. It prints the raw COBS
frame length, a hex preview of the frame contents, the decoded payload size,
and optionally dumps the raw bytes to disk.  This helps pinpoint whether frame
truncation happens on the host side (USB/UART) or inside the firmware.

Usage examples:

    python3 diagnose_flex_frames.py --port /dev/ttyUSB0 --capcode 37137 \\
        --frequency 931.9375 --power 10 --message "Hello FLEX" --wait 40 \\
        --dump-dir ./frames --verbose

If the device returns a truncated COBS frame, the script will highlight the
unexpected size before raising the corresponding FlexError.
"""

import argparse
import sys
from datetime import datetime
from pathlib import Path

SCRIPT_DIR = Path(__file__).resolve().parent
LIB_ROOT = SCRIPT_DIR.parent
if str(LIB_ROOT) not in sys.path:
    sys.path.insert(0, str(LIB_ROOT))

from FlexDevice import (
    FlexDevice,
    FlexError,
    FlexTimeoutError,
    FlexNackError,
    FlexRejectedError,
    FlexTxFailedError,
    _cobs_decode,
    _parse_packet,
    PACKET_FIXED_SIZE,
)


class DiagnosticFlexDevice(FlexDevice):
    """FlexDevice variant that logs raw frames for diagnostics."""

    def __init__(self, *args, dump_dir: Path | None = None, **kwargs):
        self.dump_dir = Path(dump_dir) if dump_dir else None
        if self.dump_dir:
            self.dump_dir.mkdir(parents=True, exist_ok=True)
        self._frame_index = 0
        super().__init__(*args, **kwargs)

    def _recv_packet(self, timeout_ms: int):
        frame = self._read_frame(timeout_ms)
        self._frame_index += 1
        frame_len = len(frame)

        head = " ".join(f"{b:02X}" for b in frame[:16])
        tail = (
            " ".join(f"{b:02X}" for b in frame[-16:])
            if frame_len > 16
            else head
        )

        print(
            f"FRAME {self._frame_index:03d}: raw_len={frame_len}  "
            f"head={head}"
        )
        if frame_len > 16:
            print(f"             tail={tail}")

        if self.dump_dir:
            timestamp = datetime.utcnow().strftime("%Y%m%d-%H%M%S")
            dump_path = (
                self.dump_dir
                / f"frame_{self._frame_index:03d}_{frame_len}_{timestamp}.bin"
            )
            dump_path.write_bytes(frame)
            print(f"             saved raw frame to {dump_path}")

        try:
            decoded = _cobs_decode(frame)
        except FlexError as exc:
            print(f"             COBS decode error: {exc}")
            raise

        decoded_len = len(decoded)
        print(f"             decoded_len={decoded_len}")

        if decoded_len != PACKET_FIXED_SIZE:
            print(
                f"             WARNING: decoded payload size {decoded_len} "
                f"(expected {PACKET_FIXED_SIZE})"
            )

        try:
            pkt = _parse_packet(decoded)
        except FlexError as exc:
            print(f"             Packet parse error: {exc}")
            raise

        if self.verbose:
            uuid_hex = decoded[4:20].hex()
            print(
                f"             packet type=0x{pkt['type']:02X} "
                f"opcode=0x{pkt['opcode']:02X} seq={pkt['seq']} "
                f"uuid={uuid_hex}"
            )

        return pkt


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--port", required=True, help="Serial port path")
    parser.add_argument("--baudrate", type=int, default=115200)
    parser.add_argument("--capcode", type=int, required=True)
    parser.add_argument("--frequency", type=float, required=True)
    parser.add_argument("--power", type=int, default=10)
    parser.add_argument("--message", required=True)
    parser.add_argument(
        "--wait",
        type=float,
        default=30.0,
        help="Timeout (seconds) waiting for TX_DONE/TX_FAILED (default: 30)",
    )
    parser.add_argument(
        "--timeout",
        type=float,
        default=5.0,
        help="Serial read timeout used for ACK (seconds, default: 5)",
    )
    parser.add_argument(
        "--dump-dir",
        type=Path,
        help="Optional directory to store raw frame dumps",
    )
    parser.add_argument(
        "--reset-lines",
        action="store_true",
        help="Toggle DTR/RTS before sending (helps re-sync some adapters)",
    )
    parser.add_argument(
        "--verbose",
        action="store_true",
        help="Enable verbose FlexDevice logging",
    )
    return parser.parse_args()


def main() -> int:
    args = parse_args()

    message = args.message
    if len(message) > 255:
        print(
            "WARNING: message longer than 255 characters; truncating for protocol"
        )
        message = message[:255]

    with DiagnosticFlexDevice(
        port=args.port,
        baudrate=args.baudrate,
        timeout=args.timeout,
        verbose=args.verbose,
        dump_dir=args.dump_dir,
    ) as dev:
        if args.reset_lines:
            dev._serial.setDTR(False)  # type: ignore[attr-defined]
            dev._serial.setRTS(True)   # type: ignore[attr-defined]
            time_ms = 100
            print(f"Toggled DTR/RTS, sleeping {time_ms} ms to resync UART")
            import time as _time_module

            _time_module.sleep(time_ms / 1000.0)
            dev._serial.setRTS(False)  # type: ignore[attr-defined]

        print(
            f"Sending message (capcode={args.capcode}, freq={args.frequency}, "
            f"power={args.power}, length={len(message)})"
        )

        try:
            uuid = dev.send_message_wait(
                capcode=args.capcode,
                frequency=args.frequency,
                power=args.power,
                message=message,
                wait_timeout=args.wait,
            )
        except FlexNackError as exc:
            print(f"NACK from device: status=0x{exc.status:02X}")
            return 2
        except FlexRejectedError as exc:
            print(f"ACK rejected: status=0x{exc.status:02X}")
            return 3
        except FlexTxFailedError as exc:
            print(f"TX_FAILED result=0x{exc.result:02X}")
            return 4
        except FlexTimeoutError as exc:
            print(f"Timeout: {exc}")
            return 5
        except FlexError as exc:
            print(f"Flex error: {exc}")
            return 6

        print(f"TX_DONE SUCCESS uuid={uuid}")
        return 0


if __name__ == "__main__":
    sys.exit(main())
