#!/usr/bin/env python3
"""Capture current from the Nordic PPK2 into a CSV.

    python tools/ppk2_capture.py --seconds 30 --rate 1000 --out evidence/led_blue.csv
    python tools/ppk2_capture.py --seconds 120 --rate 100 --source-mv 3300 --out psm.csv

The PPK2 samples at 100 kHz; --rate is the row rate of the CSV, each row the mean of
the samples in its window, in microamps. Ampere-meter mode by default (the PPK2 in
series with the device's own supply); --source-mv puts it in source-meter mode at that
voltage, which is how the ESP32 (e15) and the NB-IoT module (e16) are metered. A
Raspberry Pi cannot be powered or metered this way (about 1 A), and the tool does not
pretend otherwise.

Package: ppk2-api, pinned in requirements.txt (0.9.2 on PyPI, Wednesday 14 October
2026, GPLv2). The method names below follow that package's README; the first run on
the bench confirms them.
"""
import argparse
import csv
import sys
import time

try:
    from ppk2_api.ppk2_api import PPK2_API
except ImportError:
    PPK2_API = None

PPK2_HZ = 100000


def main():
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("--seconds", type=float, required=True)
    parser.add_argument("--rate", type=int, default=1000, help="CSV rows per second")
    parser.add_argument("--out", required=True)
    parser.add_argument("--port", help="serial port; the first PPK2 found if omitted")
    parser.add_argument("--source-mv", type=int, help="source-meter mode at this voltage")
    args = parser.parse_args()

    if PPK2_API is None:
        raise SystemExit("ppk2_capture: pip install -r requirements.txt (ppk2-api)")
    if args.rate < 1 or args.rate > PPK2_HZ:
        raise SystemExit("ppk2_capture: --rate must be 1..%d" % PPK2_HZ)
    port = args.port
    if port is None:
        ports = PPK2_API.list_devices()
        if not ports:
            raise SystemExit("ppk2_capture: no PPK2 found")
        port = ports[0]

    ppk2 = PPK2_API(port)
    ppk2.get_modifiers()
    if args.source_mv is not None:
        ppk2.use_source_meter()
        ppk2.set_source_voltage(args.source_mv)
        ppk2.toggle_DUT_power("ON")
    else:
        ppk2.use_ampere_meter()

    window = PPK2_HZ // args.rate
    pending = []
    rows = []
    sample_index = 0
    ppk2.start_measuring()
    started = time.monotonic()
    try:
        while time.monotonic() - started < args.seconds:
            data = ppk2.get_data()
            if not data:
                time.sleep(0.001)
                continue
            samples, _digital = ppk2.get_samples(data)
            for value in samples:
                pending.append(value)
                if len(pending) == window:
                    rows.append((sample_index / PPK2_HZ, sum(pending) / window))
                    sample_index += window
                    pending = []
    finally:
        ppk2.stop_measuring()
        if args.source_mv is not None:
            ppk2.toggle_DUT_power("OFF")

    with open(args.out, "w", newline="", encoding="ascii") as fh:
        writer = csv.writer(fh)
        writer.writerow(["t_s", "current_ua"])
        for t_s, ua in rows:
            writer.writerow(["%.6f" % t_s, "%.3f" % ua])
    if rows:
        values = [ua for _t, ua in rows]
        print("ppk2_capture: %d rows, mean %.1f uA, min %.1f uA, max %.1f uA, written %s" %
              (len(rows), sum(values) / len(values), min(values), max(values), args.out))
    else:
        print("ppk2_capture: no samples received")
        return 1
    return 0


if __name__ == "__main__":
    sys.exit(main())
