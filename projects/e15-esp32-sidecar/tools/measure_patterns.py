#!/usr/bin/env python3
"""Measure the mean current of each LED pattern on the ESP32 with the PPK2.

    python projects/e15-esp32-sidecar/tools/measure_patterns.py \\
        --serial /dev/serial/by-id/<cable> --out projects/e15-esp32-sidecar/evidence

For each of O (LED off, the baseline), A, B, C and D the harness sends the one-letter
command over the serial link, waits for the firmware's "pattern X" echo, lets the
pattern settle for --settle seconds, captures --seconds of current (20 by default) and
keeps the mean in milliamps; the whole sweep is repeated --runs times (three by
default). It writes two files into --out:

    pattern_costs.csv          pattern,run,mean_ma,board_mean_ma,samples     every capture
    pattern_costs_summary.csv  pattern,mean_ma,spread_pct,runs              per pattern

where mean_ma is the pattern's mean above the baseline of the same run and spread_pct
is (max - min) / mean over the runs, the acceptance quantity of the chapter: it must be
within 5 percent for every pattern. board_mean_ma is the whole board, baseline included.

The PPK2 is the ESP32's only supply, in source-meter mode at 3300 mV, which this tool
sets before the DUT is enabled; the ESP32's own USB connector stays empty (the two
supplies trap, kit lab P10). The capture loop follows tools/ppk2_capture.py and uses
the same package: ppk2-api 0.9.2 (GPLv2), class PPK2_API, methods list_devices,
get_modifiers, use_source_meter, set_source_voltage, toggle_DUT_power, start_measuring,
get_data, get_samples, stop_measuring, read from the package source on Wednesday
14 October 2026; the first run on the bench confirms them.
"""
import argparse
import csv
import sys
import time
from pathlib import Path

try:
    from ppk2_api.ppk2_api import PPK2_API
except ImportError:
    PPK2_API = None

try:
    import serial
except ImportError:
    serial = None

PPK2_HZ = 100000
PATTERNS = ("O", "A", "B", "C", "D")
NAMES = {"O": "OFF", "A": "PATTERN_A", "B": "PATTERN_B", "C": "PATTERN_C", "D": "PATTERN_D"}


def command(link, letter, tries=3):
    """Sends one letter and returns True when the firmware echoes it back."""
    for _ in range(tries):
        link.reset_input_buffer()
        link.write((letter + "\n").encode("ascii"))
        reply = link.readline().decode("ascii", "replace").strip()
        if reply == "pattern " + letter:
            return True
        time.sleep(0.2)
    return False


def capture_mean_ma(ppk2, seconds):
    """The mean of every sample in the window, in milliamps, and the sample count."""
    total = 0.0
    count = 0
    ppk2.start_measuring()
    started = time.monotonic()
    try:
        while time.monotonic() - started < seconds:
            data = ppk2.get_data()
            if not data:
                time.sleep(0.001)
                continue
            samples, _digital = ppk2.get_samples(data)
            total += sum(samples)
            count += len(samples)
    finally:
        ppk2.stop_measuring()
    if count == 0:
        raise SystemExit("measure_patterns: the PPK2 delivered no samples")
    return total / count / 1000.0, count


def main():
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("--serial", required=True, help="the cable to the ESP32 UART2")
    parser.add_argument("--baud", type=int, default=115200)
    parser.add_argument("--out", required=True, help="the evidence folder")
    parser.add_argument("--port", help="the PPK2 serial port; the first found if omitted")
    parser.add_argument("--source-mv", type=int, default=3300)
    parser.add_argument("--seconds", type=float, default=20.0)
    parser.add_argument("--settle", type=float, default=2.0)
    parser.add_argument("--runs", type=int, default=3)
    parser.add_argument("--limit-pct", type=float, default=5.0, help="the acceptance spread")
    args = parser.parse_args()

    if PPK2_API is None or serial is None:
        raise SystemExit("measure_patterns: pip install -r requirements.txt (ppk2-api, pyserial)")
    port = args.port
    if port is None:
        ports = PPK2_API.list_devices()
        if not ports:
            raise SystemExit("measure_patterns: no PPK2 found")
        port = ports[0]
    out = Path(args.out)
    out.mkdir(parents=True, exist_ok=True)

    ppk2 = PPK2_API(port)
    ppk2.get_modifiers()
    ppk2.use_source_meter()
    ppk2.set_source_voltage(args.source_mv)      # set before the DUT sees power
    ppk2.toggle_DUT_power("ON")
    time.sleep(3.0)                               # the ESP32 boots and prints its banner
    link = serial.Serial(args.serial, args.baud, timeout=1.0)
    rows = []
    try:
        for run in range(1, args.runs + 1):
            baseline = None
            for letter in PATTERNS:
                if not command(link, letter):
                    raise SystemExit("measure_patterns: no echo for %s in run %d" % (letter, run))
                time.sleep(args.settle)
                board_ma, count = capture_mean_ma(ppk2, args.seconds)
                if letter == "O":
                    baseline = board_ma
                above = board_ma - baseline
                rows.append((NAMES[letter], run, above, board_ma, count))
                print("run %d %-9s board %.3f mA, above baseline %.3f mA, %d samples"
                      % (run, NAMES[letter], board_ma, above, count))
    finally:
        command(link, "O")
        link.close()
        ppk2.toggle_DUT_power("OFF")

    with (out / "pattern_costs.csv").open("w", newline="", encoding="ascii") as fh:
        w = csv.writer(fh)
        w.writerow(["pattern", "run", "mean_ma", "board_mean_ma", "samples"])
        for name, run, above, board_ma, count in rows:
            w.writerow([name, run, "%.4f" % above, "%.4f" % board_ma, count])

    worst = 0.0
    with (out / "pattern_costs_summary.csv").open("w", newline="", encoding="ascii") as fh:
        w = csv.writer(fh)
        w.writerow(["pattern", "mean_ma", "spread_pct", "runs"])
        for letter in PATTERNS:
            means = [r[2] if letter != "O" else r[3] for r in rows if r[0] == NAMES[letter]]
            mean = sum(means) / len(means)
            spread = 100.0 * (max(means) - min(means)) / mean if mean > 0.0 else 0.0
            worst = max(worst, spread) if letter != "O" else worst
            w.writerow([NAMES[letter], "%.4f" % mean, "%.2f" % spread, len(means)])
            print("%-9s mean %.3f mA, spread %.2f percent over %d runs"
                  % (NAMES[letter], mean, spread, len(means)))
    verdict = "PASS" if worst <= args.limit_pct else "FAIL"
    print("acceptance: worst spread %.2f percent against %.1f percent: %s"
          % (worst, args.limit_pct, verdict))
    return 0 if verdict == "PASS" else 1


if __name__ == "__main__":
    sys.exit(main())
