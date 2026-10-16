#!/usr/bin/env python3
"""Generate the e07 replay file and its oracle. Python is allowed here as a generator.

    python projects/e07-iks4a1-narrator/tools/make_replay.py

Writes projects/e07-iks4a1-narrator/replay/synthetic_day.csv: one synthetic day of
1440 rows, one a minute, with the header t_s,tag,v0,...,v8 in the value order fixed at
the top of src/main.cpp: v0 pressure hPa, v1 the pressure 30 rows earlier (the oldest
row available during the first 30 minutes, as the reader's ring buffer does), v2
temperature C, v3 humidity percent, v4 acceleration magnitude g, v5 rotation rate
deg/s, v6 magnetic field uT, v7 and v8 the second IMU's two magnitudes.

The pressure carries a 12 hour tide of 1 hPa, whose steepest 30 minute change is well
under the 1 hPa threshold, a front that drops 12 hPa between hour 9 and hour 14, and
a recovery of 13 hPa between hour 16.5 and hour 22, so all three trends occur. Motion
is quiet at 1.00 g with a few seeded episodes of handling. The walk is seeded, so
running the script twice writes the same bytes.

Also writes tests/oracle/e07.expected.csv with the header t_s,tag,rule,model: rule is
the decision of rules_e07.c applied in Python with the same comparisons on the same
two numbers, and model is what the CI stub answers on the rows the loop asks it (every
60th row, the first included): the grammar's first expansion of every rule, which for
grammars/e07_pressure.gbnf is {"trend":"STABLE","summary":". . ."}, CSV-quoted the way
common/c/src/elog.c writes it. The other rows carry an empty model cell, because the
loop logs the rule alone there. The printed summary gives the trend counts over all
rows and over the 24 asked rows; the STABLE fraction of the asked rows is the
agreement the stub reaches in CI, and the chapter quotes it from this output.
"""
import math
import random
from pathlib import Path

HERE = Path(__file__).resolve().parent
PROJECT = HERE.parent
REPO = PROJECT.parent.parent
REPLAY = PROJECT / "replay" / "synthetic_day.csv"
ORACLE = REPO / "tests" / "oracle" / "e07.expected.csv"
N = 1440
EVERY = 60
TAG = "iks4a1"
STUB_TEXT = '"{""trend"":""STABLE"",""summary"":"". . .""}"'


def rule(v0, v1):
    """The same two comparisons as rules_e07.c, in the same order."""
    d = v0 - v1
    if d > 1.0:
        return "RISING"
    if d < -1.0:
        return "FALLING"
    return "STABLE"


def pressure_shape(minute):
    """hPa before noise: tide, a falling front, a recovery."""
    h = minute / 60.0
    p = 1013.0 + 1.0 * math.sin(2.0 * math.pi * (h / 12.0 + 0.1))
    if 9.0 <= h < 14.0:
        p -= 12.0 * (h - 9.0) / 5.0
    elif 14.0 <= h < 16.5:
        p -= 12.0
    elif 16.5 <= h < 22.0:
        p -= 12.0 - 13.0 * (h - 16.5) / 5.5
    elif h >= 22.0:
        p += 1.0
    return p


def day(n, seed=2026):
    rng = random.Random(seed)
    episodes = []
    for _ in range(6):
        start = rng.randint(30, n - 40)
        episodes.append((start, start + rng.randint(2, 9), rng.uniform(1.05, 1.6),
                         rng.uniform(15.0, 90.0)))
    rows = []
    pressures = []
    for i in range(n):
        h = i / 60.0
        p = round(pressure_shape(i) + rng.gauss(0.0, 0.03), 2)
        pressures.append(p)
        p_old = pressures[max(0, i - 30)]
        temp = round(21.5 + 2.5 * math.sin(2.0 * math.pi * (h - 9.0) / 24.0)
                     + rng.gauss(0.0, 0.05), 2)
        hum = round(52.0 - 8.0 * math.sin(2.0 * math.pi * (h - 9.0) / 24.0)
                    + rng.gauss(0.0, 0.3), 1)
        acc = 1.0 + rng.gauss(0.0, 0.004)
        gyr = 0.3 + abs(rng.gauss(0.0, 0.1))
        for start, end, a, g in episodes:
            if start <= i < end:
                acc = a + rng.gauss(0.0, 0.02)
                gyr = g + rng.gauss(0.0, 2.0)
        mag = 48.0 + rng.gauss(0.0, 0.3)
        acc2 = acc + rng.gauss(0.0, 0.004)
        gyr2 = gyr + abs(rng.gauss(0.0, 0.1))
        rows.append((60.0 * i, p, p_old, temp, hum, round(acc, 3), round(gyr, 2),
                     round(mag, 1), round(acc2, 3), round(gyr2, 2)))
    return rows


def main():
    rows = day(N)
    REPLAY.parent.mkdir(parents=True, exist_ok=True)
    ORACLE.parent.mkdir(parents=True, exist_ok=True)
    counts = {"STABLE": 0, "RISING": 0, "FALLING": 0}
    asked = {"STABLE": 0, "RISING": 0, "FALLING": 0}
    with REPLAY.open("w", newline="\n", encoding="ascii") as f_rep, \
            ORACLE.open("w", newline="\n", encoding="ascii") as f_orc:
        f_rep.write("t_s,tag,v0,v1,v2,v3,v4,v5,v6,v7,v8\n")
        f_orc.write("t_s,tag,rule,model\n")
        for i, (t_s, p, p_old, temp, hum, acc, gyr, mag, acc2, gyr2) in enumerate(rows):
            r = rule(p, p_old)
            counts[r] += 1
            model = ""
            if i % EVERY == 0:
                asked[r] += 1
                model = STUB_TEXT
            f_rep.write(f"{t_s:.1f},{TAG},{p:.2f},{p_old:.2f},{temp:.2f},{hum:.1f},"
                        f"{acc:.3f},{gyr:.2f},{mag:.1f},{acc2:.3f},{gyr2:.2f}\n")
            f_orc.write(f"{t_s:.6f},{TAG},{r},{model}\n")
    n_asked = sum(asked.values())
    print(f"wrote {REPLAY.relative_to(REPO).as_posix()} and "
          f"{ORACLE.relative_to(REPO).as_posix()}")
    for k in counts:
        print(f"{k:8s} {counts[k]:5d} of {N} rows  {100.0 * counts[k] / N:5.1f} percent;"
              f"  asked rows {asked[k]:3d} of {n_asked}")
    print(f"stub agreement over the {n_asked} asked rows (the STABLE fraction): "
          f"{asked['STABLE']} of {n_asked}, {100.0 * asked['STABLE'] / n_asked:.1f} percent")


if __name__ == "__main__":
    main()
