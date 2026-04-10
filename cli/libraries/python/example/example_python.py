#!/usr/bin/env python3
"""
example_python.py - FlexDevice library usage example

Usage:
    python3 example_python.py /dev/ttyUSB0 1234567 "Hello World"
    python3 example_python.py -f 929.6625 -p 15 -w /dev/ttyACM0 37137 "Test"
"""

import sys
import argparse
from FlexDevice import (FlexDevice, FlexError, FlexTimeoutError,
                         FlexNackError, FlexRejectedError, FlexTxFailedError)


def main():
    parser = argparse.ArgumentParser(description='Send FLEX message via binary protocol')
    parser.add_argument('device',  help='Serial device (e.g., /dev/ttyUSB0)')
    parser.add_argument('capcode', type=int, help='FLEX capcode')
    parser.add_argument('message', help='Message to send')
    parser.add_argument('-b', '--baudrate', type=int, default=115200,
                        help='Serial baudrate (default: 115200)')
    parser.add_argument('-f', '--freq', type=float, default=931.9375,
                        help='Frequency in MHz (default: 931.9375)')
    parser.add_argument('-p', '--power', type=int, default=10,
                        help='TX power in dBm (default: 10)')
    parser.add_argument('-m', '--mail-drop', action='store_true',
                        help='Enable mail drop flag')
    parser.add_argument('-w', '--wait', action='store_true',
                        help='Wait for TX_DONE event')
    parser.add_argument('-v', '--verbose', action='store_true',
                        help='Verbose output')
    args = parser.parse_args()

    print(f"Connecting to {args.device} @ {args.baudrate} baud")
    print(f"Capcode: {args.capcode}  Freq: {args.freq} MHz  "
          f"Power: {args.power} dBm  Mail drop: {args.mail_drop}")
    print(f"Message: \"{args.message}\" ({len(args.message.encode())} bytes)\n")

    try:
        with FlexDevice(args.device, baudrate=args.baudrate, verbose=args.verbose) as dev:
            if args.wait:
                uuid = dev.send_message_wait(
                    capcode=args.capcode,
                    frequency=args.freq,
                    power=args.power,
                    message=args.message,
                    mail_drop=args.mail_drop,
                    wait_timeout=30.0,
                )
            else:
                uuid = dev.send_message(
                    capcode=args.capcode,
                    frequency=args.freq,
                    power=args.power,
                    message=args.message,
                    mail_drop=args.mail_drop,
                )

            print(f"Message sent. UUID: {uuid}")

            if args.verbose and dev.last_response:
                rsp = dev.last_response
                print(f"Last response: type=0x{rsp['type']:02X} opcode=0x{rsp['opcode']:02X}")
                if rsp.get('ts_flags', 0) & 0x01:
                    print(f"ESP32 timestamp: {rsp['ts_unix']}.{rsp['ts_ms']:03d} "
                          f"(UTC{rsp['ts_tz'] * 0.5:+.1f})")

    except FlexTimeoutError as e:
        print(f"Timeout: {e}", file=sys.stderr)
        return 1
    except FlexNackError as e:
        print(f"NACK: {e}", file=sys.stderr)
        return 1
    except FlexRejectedError as e:
        print(f"Rejected: {e}", file=sys.stderr)
        return 1
    except FlexTxFailedError as e:
        print(f"TX failed: {e}", file=sys.stderr)
        return 1
    except FlexError as e:
        print(f"Error: {e}", file=sys.stderr)
        return 1

    return 0


if __name__ == '__main__':
    sys.exit(main())
