#!/usr/bin/env python3
"""Reduce one PPK2 capture of an NB-IoT power-saving-mode cycle to the numbers the e16
loop reads, and report whether the cycle repeated inside the acceptance condition.

    python tools/ppk2_capture.py --seconds 1200 --rate 100 --out psm.csv
    python projects/e16-nbiot-sleep-interval/tools/profile_summarise.py psm.csv \\
        --capacity 2000 --target 365 --granted 3600

The capture is the one written by tools/ppk2_capture.py: a header line t_s,current_ua
and one row per window, the mean current of that window in microamps. The PPK2 is in
ampere-meter mode, in series with the module's own supply rail. It is never on the
Raspberry Pi: the instrument sources and measures up to about 1 A and a Pi under load
draws more, so no figure here is a Pi figure and the chapter says so.

What this tool does, in order:

  1. the sleep floor is the median of the lowest fifth of the samples, which is the PSM
     current of the module and its board together, never the module alone;
  2. a sample above floor + 5 percent of the span to the peak is awake; a run of such
     samples is one wake window, with gaps under --bridge seconds bridged and runs
     under --min-wake seconds dropped, so one ragged transmission is one window;
  3. each window gives its length in seconds, its charge in microamp hours (the
     integral of the current over the window, floor included) and its peak in
     milliamps;
  4. the cycle length is start to start between consecutive windows;
  5. the acceptance condition of the chapter: over the first --cycles windows, the
     spread of the charge per wake, (max - min) / mean, must be at most 10 percent.
     The spread is printed whether it passes or not, because the spread is the result.

The last line printed is the scenario row for replay/profiles_100.csv, so a measured
profile can be put beside the modelled ones with no editing by hand.

--self-check builds a synthetic capture with known figures, summarises it and compares
the recovered figures with the ones it was built from. It needs no instrument and is
how this tool is exercised before the PPK2 is wired.
"""
import argparse
import csv
import math
import random
import statistics
import sys
import tempfile
from pathlib import Path

ACCEPT_SPREAD = 0.10
TAG = "ppk2_cycle"


def read_capture(path):
    """Returns (times, currents) in seconds and microamps, and refuses a header that is
    not the one tools/ppk2_capture.py writes."""
    times, currents = [], []
    with open(path, newline="", encoding="ascii") as fh:
        reader = csv.reader(fh)
        header = next(reader, None)
        if header is None or header[:2] != ["t_s", "current_ua"]:
            raise SystemExit("profile_summarise: %s is not a ppk2_capture.py CSV "
                             "(header t_s,current_ua)" % path)
        for row in reader:
            if len(row) < 2:
                continue
            times.append(float(row[0]))
            currents.append(float(row[1]))
    if len(times) < 10:
        raise SystemExit("profile_summarise: %s holds %d rows, too few to segment"
                         % (path, len(times)))
    return times, currents


def sample_period(times):
    steps = [times[i + 1] - times[i] for i in range(len(times) - 1)]
    return statistics.median(steps)


def sleep_floor_ua(currents):
    ordered = sorted(currents)
    low = ordered[:max(1, len(ordered) // 5)]
    return statistics.median(low)


def wake_windows(times, currents, floor, dt, bridge_s, min_wake_s):
    """Index ranges of the wake windows, gaps under bridge_s bridged."""
    peak = max(currents)
    threshold = floor + 0.05 * (peak - floor)
    bridge = max(1, int(round(bridge_s / dt)))
    runs = []
    start = None
    quiet = 0
    for i, value in enumerate(currents):
        if value > threshold:
            if start is None:
                start = i
            quiet = 0
        elif start is not None:
            quiet += 1
            if quiet >= bridge:
                runs.append((start, i - quiet + 1))
                start = None
                quiet = 0
    if start is not None:
        runs.append((start, len(currents)))
    return threshold, [(a, b) for a, b in runs if (b - a) * dt >= min_wake_s]


def summarise(times, currents, bridge_s, min_wake_s):
    dt = sample_period(times)
    floor = sleep_floor_ua(currents)
    threshold, runs = wake_windows(times, currents, floor, dt, bridge_s, min_wake_s)
    wakes = []
    for a, b in runs:
        charge_uah = sum(currents[a:b]) * dt / 3600.0
        wakes.append({"t_start": times[a], "active_s": (b - a) * dt,
                      "active_uah": charge_uah, "peak_ma": max(currents[a:b]) / 1000.0})
    for i in range(len(wakes) - 1):
        wakes[i]["cycle_s"] = wakes[i + 1]["t_start"] - wakes[i]["t_start"]
    return {"dt": dt, "floor_ua": floor, "threshold_ua": threshold, "wakes": wakes}


def spread(values):
    if not values:
        return float("nan")
    mean = sum(values) / len(values)
    return (max(values) - min(values)) / mean if mean > 0.0 else float("nan")


def report(summary, args):
    wakes = summary["wakes"]
    print("samples every %.4f s, sleep floor %.2f uA, wake threshold %.2f uA, "
          "%d wake windows" % (summary["dt"], summary["floor_ua"],
                               summary["threshold_ua"], len(wakes)))
    print("%-6s %10s %12s %10s %10s" % ("wake", "start s", "charge uAh", "length s",
                                        "peak mA"))
    for i, w in enumerate(wakes, 1):
        print("%-6d %10.2f %12.3f %10.2f %10.3f" % (i, w["t_start"], w["active_uah"],
                                                    w["active_s"], w["peak_ma"]))
    cycles = [w["cycle_s"] for w in wakes if "cycle_s" in w]
    if cycles:
        print("cycle length start to start: %s s" %
              ", ".join("%.1f" % c for c in cycles))
    head = wakes[:args.cycles]
    if len(head) < args.cycles:
        print("only %d of the %d windows the acceptance condition asks for; the spread "
              "below is over those %d" % (len(head), args.cycles, len(head)))
    charges = [w["active_uah"] for w in head]
    s = spread(charges)
    print("charge per wake over %d windows: mean %.3f uAh, spread %.1f percent, "
          "acceptance at most %.0f percent: %s" %
          (len(head), sum(charges) / len(charges) if charges else float("nan"),
           100.0 * s, 100.0 * ACCEPT_SPREAD,
           "pass" if s <= ACCEPT_SPREAD else "FAIL"))
    mean_charge = sum(charges) / len(charges) if charges else 0.0
    mean_active = sum(w["active_s"] for w in head) / len(head) if head else 0.0
    print("scenario row for replay/profiles_100.csv (t_s,tag,v0,v1,v2,v3,v4,v5):")
    print("0.0,%s,%.1f,%.1f,%.1f,%.0f,%.0f,%.0f" %
          (TAG, summary["floor_ua"], mean_charge, mean_active, args.capacity,
           args.target, args.granted))
    return 0 if s <= ACCEPT_SPREAD else 1


def synthetic(path, floor_ua=12.0, wake_ua=9000.0, wake_s=20.0, cycle_s=120.0,
              cycles=5, rate=10.0, seed=2026):
    """A capture with known figures: a flat floor, a flat wake of wake_s seconds every
    cycle_s seconds, and a 2 percent jitter on the wake amplitude so that the spread is
    neither zero nor outside the acceptance condition."""
    rng = random.Random(seed)
    dt = 1.0 / rate
    rows = []
    n = int(round(cycles * cycle_s * rate))
    amplitudes = [wake_ua * (1.0 + rng.uniform(-0.02, 0.02)) for _ in range(cycles + 1)]
    for i in range(n):
        t = i * dt
        k = int(t // cycle_s)
        rows.append((t, amplitudes[k] if (t - k * cycle_s) < wake_s else floor_ua))
    with open(path, "w", newline="", encoding="ascii") as fh:
        writer = csv.writer(fh)
        writer.writerow(["t_s", "current_ua"])
        for t, ua in rows:
            writer.writerow(["%.6f" % t, "%.3f" % ua])
    return {"floor_ua": floor_ua, "active_s": wake_s, "cycle_s": cycle_s,
            "active_uah": wake_ua * wake_s / 3600.0}


def self_check(args):
    tmp = Path(tempfile.gettempdir()) / "e16_profile_selfcheck.csv"
    want = synthetic(tmp)
    times, currents = read_capture(tmp)
    summary = summarise(times, currents, args.bridge, args.min_wake)
    report(summary, args)
    wakes = summary["wakes"]
    problems = []
    if len(wakes) != 5:
        problems.append("expected 5 wake windows, found %d" % len(wakes))
    if abs(summary["floor_ua"] - want["floor_ua"]) > 0.01:
        problems.append("floor %.3f uA, expected %.3f" % (summary["floor_ua"],
                                                          want["floor_ua"]))
    for i, w in enumerate(wakes, 1):
        if abs(w["active_s"] - want["active_s"]) > 2.0 * summary["dt"]:
            problems.append("wake %d length %.2f s, expected %.2f" %
                            (i, w["active_s"], want["active_s"]))
        if abs(w["active_uah"] - want["active_uah"]) / want["active_uah"] > 0.03:
            problems.append("wake %d charge %.3f uAh, expected %.3f within 3 percent" %
                            (i, w["active_uah"], want["active_uah"]))
    for w in wakes[:-1]:
        if abs(w["cycle_s"] - want["cycle_s"]) > 2.0 * summary["dt"]:
            problems.append("cycle %.2f s, expected %.2f" % (w["cycle_s"],
                                                             want["cycle_s"]))
    for item in problems:
        print("FAIL " + item)
    print("self-check: %d problems, synthetic capture at %s" % (len(problems), tmp))
    return 1 if problems else 0


def main():
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("capture", nargs="?", help="the ppk2_capture.py CSV")
    parser.add_argument("--capacity", type=float, default=2000.0,
                        help="the cell in mAh, stated not measured")
    parser.add_argument("--target", type=float, default=365.0, help="battery target, days")
    parser.add_argument("--granted", type=float, default=0.0,
                        help="the PSM window the network granted, seconds; "
                             "edgellm-e16 --at prints it")
    parser.add_argument("--cycles", type=int, default=5,
                        help="windows the acceptance condition spans")
    parser.add_argument("--bridge", type=float, default=1.0,
                        help="gaps shorter than this stay inside one wake, seconds")
    parser.add_argument("--min-wake", type=float, default=0.5,
                        help="runs shorter than this are not wakes, seconds")
    parser.add_argument("--self-check", action="store_true",
                        help="summarise a synthetic capture and check the recovery")
    args = parser.parse_args()
    if args.self_check:
        return self_check(args)
    if args.capture is None:
        parser.error("a capture file, or --self-check")
    times, currents = read_capture(args.capture)
    if math.isnan(sample_period(times)):
        raise SystemExit("profile_summarise: the timestamps are not usable")
    return report(summarise(times, currents, args.bridge, args.min_wake), args)


if __name__ == "__main__":
    sys.exit(main())
