"""Appends the board's USB console to a log file, reopening the port across resets.

usage: serial_log.py <port> <logfile> [seconds]
"""
import sys
import time

import serial

port, path = sys.argv[1], sys.argv[2]
deadline = time.time() + (float(sys.argv[3]) if len(sys.argv) > 3 else 1e9)
REOPEN_DELAY_S = 0.5

with open(path, "a", buffering=1) as log:
    while time.time() < deadline:
        try:
            # Opening without toggling DTR/RTS so the board is not reset.
            s = serial.Serial()
            s.port, s.baudrate, s.timeout, s.dtr, s.rts = port, 115200, 0.5, False, False
            s.open()
            while time.time() < deadline:
                line = s.readline()
                if line:
                    log.write(time.strftime("%H:%M:%S ") + line.decode("utf-8", "replace"))
        except (serial.SerialException, OSError):
            time.sleep(REOPEN_DELAY_S)
