#!/usr/bin/env python3
"""Soft-reset the lcd-2 board into download mode over its USB-Serial/JTAG port.

The ESP32-S3 TRM (ch. 33) defines a DTR/RTS handshake that enters download
mode without buttons. We have no RST-observable button flow that works
reliably on this board; this pulse does, and keeps the port parked so
esptool can flash and `run` in the same session.

Usage: python3 tools/esp32/lcd2_download_mode.py [/dev/cu.usbmodemXXXX]
(picks the first /dev/cu.usbmodem* if no port is given)
"""

import glob
import sys
import time

import serial


def enter_download_mode(port: str) -> None:
    s = serial.Serial(port, 115200, exclusive=True)
    s.dtr = False
    time.sleep(0.1)  # noqa: E701
    s.rts = False
    time.sleep(0.1)  # noqa: E701
    s.dtr = True
    time.sleep(0.1)  # noqa: E701  set download flag
    s.rts = False
    time.sleep(0.1)  # noqa: E701  propagate DTR
    s.rts = True
    time.sleep(0.1)  # noqa: E701
    s.dtr = False
    time.sleep(0.1)  # noqa: E701  reset SoC
    s.rts = False
    time.sleep(0.1)  # noqa: E701  clear download flag
    s.close()


def main() -> int:
    port = sys.argv[1] if len(sys.argv) > 1 else None
    if not port:
        ports = sorted(glob.glob("/dev/cu.usbmodem*"))
        if not ports:
            print("no /dev/cu.usbmodem* port present", file=sys.stderr)
            return 1
        port = ports[0]
    enter_download_mode(port)
    print(f"download-mode pulse sent on {port}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
