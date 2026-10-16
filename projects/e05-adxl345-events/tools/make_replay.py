#!/usr/bin/env python3
"""Write the e05 replay file and its oracle. Python is allowed here as a generator.

    python projects/e05-adxl345-events/tools/make_replay.py

Writes projects/e05-adxl345-events/replay/adxl345_events.csv with the header t_s,tag,v0,v1,v2
(v0, v1, v2 the x, y and z acceleration in metres per second squared, the unit the IIO
backend delivers after raw times scale, at 50 samples per second) and
tests/oracle/e05.expected.csv with the header t_s,tag,rule,model, where rule is the decision
of src/rules.rs applied in Python with the same thresholds over the same window of 16, and
model is what the CI stub answers on the rows where the project asks the model, the onset of
a non-NONE decision: the grammar's first expansion {"decision":"NONE","why":"."}, CSV-quoted
the way the Rust log writes it. On every other row the model column is empty, because the
project logs the rule alone there; the oracle therefore carries the ask policy of
src/main.rs as well as the rule.

The script is seeded, so running it twice writes the same bytes. The record is 2000 samples,
40 seconds, of a part at rest under gravity with Gaussian noise, with thirty scripted events
sixty samples apart: ten taps (a three-sample spike of about 2.7 g along x or y), ten tilts
(the gravity vector turned by 40 to 60 degrees over twelve samples, held, turned back) and
ten drops onto foam (twelve samples at 0.05 to 0.2 g, then an impact of about 3.8 g). The
printed summary gives the count of each decision, the number of rows on which the model is
asked, and the agreement the stub reaches on those rows, which the chapter quotes from this
output.

The floating point path is the one src/rules.rs takes, in the same order of operations: the
magnitude is sqrt(x*x + y*y + z*z) / G, the tilt test is a dot product compared against
cos 30 degrees times the two norms, so the two implementations agree bit for bit on every
row and the oracle check is exact.
"""
import math
import random
from pathlib import Path

HERE = Path(__file__).resolve().parent
PROJECT = HERE.parent
REPO = PROJECT.parent.parent
REPLAY = PROJECT / "replay" / "adxl345_events.csv"
ORACLE = REPO / "tests" / "oracle" / "e05.expected.csv"

N = 2000
DT = 0.02
TAG = "adxl345"
G = 9.80665
WINDOW = 16
TAP_G = 2.0
FREEFALL_G = 0.3
COS_TILT = 0.8660254037844386
TILT_MIN_G = 0.5
NAMES = ("NONE", "TAP", "TILT", "FREEFALL")
STUB_TEXT = '"{""decision"":""NONE"",""why"":"".""}"'   # the log's quoting of the stub answer
EVENT_START = 30
EVENT_SPACING = 60
KINDS = ["tap", "tilt", "drop"] * 10


def norm(v):
    return math.sqrt(v[0] * v[0] + v[1] * v[1] + v[2] * v[2])


def rule(window):
    """src/rules.rs, E05Rule::decide, in the same order with the same arithmetic."""
    cur = window[-1]
    mags = [norm(v) / G for v in window]
    if max(mags) > TAP_G:
        return "TAP"
    cur_g = mags[-1]
    if cur_g < FREEFALL_G:
        return "FREEFALL"
    start = window[0]
    ma = norm(start)
    mb = norm(cur)
    dot = start[0] * cur[0] + start[1] * cur[1] + start[2] * cur[2]
    tilted = (len(window) >= 2 and mags[0] > TILT_MIN_G and cur_g > TILT_MIN_G
              and dot < COS_TILT * ma * mb)
    if tilted:
        return "TILT"
    return "NONE"


def build(seed=2026):
    rng = random.Random(seed)
    theta = [0.0] * N                       # gravity turned from z toward x, radians
    for k, kind in enumerate(KINDS):
        s = EVENT_START + k * EVENT_SPACING
        if kind == "tilt":
            delta = rng.choice([-1.0, 1.0]) * math.radians(rng.uniform(40.0, 60.0))
            for i in range(12):
                theta[s + i] = delta * (i + 1) / 12.0
            for i in range(24):
                theta[s + 12 + i] = delta
            for i in range(12):
                theta[s + 36 + i] = delta * (11 - i) / 12.0
    rows = []
    for i in range(N):
        gx, gz = G * math.sin(theta[i]), G * math.cos(theta[i])
        rows.append([gx + rng.gauss(0.0, 0.05), rng.gauss(0.0, 0.05), gz + rng.gauss(0.0, 0.05)])
    for k, kind in enumerate(KINDS):
        s = EVENT_START + k * EVENT_SPACING
        if kind == "tap":
            axis = rng.choice([0, 1])
            sign = rng.choice([-1.0, 1.0])
            for j, amp in enumerate((25.0, 10.0, -6.0)):
                rows[s + j][axis] += sign * amp * rng.uniform(0.9, 1.1)
        elif kind == "drop":
            for j in range(12):
                mag = rng.uniform(0.05, 0.2) * G
                d = [rng.gauss(0.0, 1.0) for _ in range(3)]
                dn = norm(d) or 1.0
                rows[s + j] = [mag * c / dn for c in d]
            rows[s + 12][2] += 28.0 * rng.uniform(0.9, 1.1)
            rows[s + 13][2] += 8.0
            rows[s + 14][2] -= 5.0
    return [[round(c, 3) for c in r] for r in rows]


def main():
    rows = build()
    REPLAY.parent.mkdir(parents=True, exist_ok=True)
    ORACLE.parent.mkdir(parents=True, exist_ok=True)
    counts = {name: 0 for name in NAMES}
    asked = 0
    asked_none = 0
    last = "NONE"
    with REPLAY.open("w", newline="\n", encoding="ascii") as f_rep, \
            ORACLE.open("w", newline="\n", encoding="ascii") as f_orc:
        f_rep.write("t_s,tag,v0,v1,v2\n")
        f_orc.write("t_s,tag,rule,model\n")
        for i, v in enumerate(rows):
            t_s = float(f"{i * DT:.2f}")
            window = rows[max(0, i - (WINDOW - 1)):i + 1]
            r = rule(window)
            counts[r] += 1
            ask = r != "NONE" and r != last
            last = r
            if ask:
                asked += 1
                if r == "NONE":
                    asked_none += 1
            model = STUB_TEXT if ask else '""'
            f_rep.write(f"{t_s:.2f},{TAG},{v[0]:.3f},{v[1]:.3f},{v[2]:.3f}\n")
            f_orc.write(f"{t_s:.6f},{TAG},{r},{model}\n")
    print(f"wrote {REPLAY.relative_to(REPO).as_posix()} and {ORACLE.relative_to(REPO).as_posix()}")
    for k, n in counts.items():
        print(f"{k:9s} {n:5d} rows  {100.0 * n / N:5.1f} percent")
    print(f"model asked on {asked} rows (the onsets); the stub answers NONE on all of them, "
          f"so its agreement on the scored rows is {asked_none} of {asked}, "
          f"{100.0 * asked_none / asked if asked else 0.0:.1f} percent")


if __name__ == "__main__":
    main()
