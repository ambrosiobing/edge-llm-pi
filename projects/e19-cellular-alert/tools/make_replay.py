#!/usr/bin/env python3
"""Generate the e19 replay file and its oracle. Python is allowed here as a generator.

    python projects/e19-cellular-alert/tools/make_replay.py

Writes projects/e19-cellular-alert/replay/alerts_50.csv with the header t_s,tag,v0,v1,v2
(the ADXL345 as the IIO backend delivers it: x, y and z in metres per second squared,
one event per second, fifty events) and tests/oracle/e19.expected.csv with the header
t_s,tag,rule,model.

The stream is not a random walk. It is a bench with an orientation that is tipped from
time to time, knocked five times and dropped twice, and the spec below says when, so
that the counts the chapter publishes are a property of a table and not of a seed. The
seed only decides the noise on each axis and the size of each knock.

  tap        a peak of 2.2 to 3.4 g along the current attitude
  freefall   a magnitude of 0.05 to 0.25 g
  tilt       the attitude changes to a new angle from vertical, by at least 35 degrees
  quiet      1 g along the current attitude, plus noise

The rule column is the classification of rules_e19.c applied in Python with the same
three thresholds and the same libm-free arithmetic, on the rounded values that are
written to the CSV, which is what the C program reads. The model column is what the CI
stub answers on an armed event, the grammar's first expansion with every character class
empty, CSV-quoted the way common/c/src/elog.c writes it; on every event the send rule
did not arm it is empty, because the model is not asked when nothing may be sent. The
oracle file is therefore also the proof that the model is asked only inside the rule's
branch: a build in which the loop asked the model on a quiet event would differ from it
on 46 rows.
"""
import math
import random
from pathlib import Path

HERE = Path(__file__).resolve().parent
PROJECT = HERE.parent
REPO = PROJECT.parent.parent
REPLAY = PROJECT / "replay" / "alerts_50.csv"
ORACLE = REPO / "tests" / "oracle" / "e19.expected.csv"

N = 50
SEED = 2026
TAG = "adxl345"
G = 9.80665
TAP_G = 2.0
FREEFALL_G = 0.3
COS30 = 0.86602540
MIN_SEND_S = 10.0
NAMES = ("NONE", "TAP", "TILT", "FREEFALL")
STUB_TEXT = '"ALERT TAP peak . g at t+::. ."'

# second -> what happens at it. A tilt carries the new angle from vertical in degrees.
SPEC = {
    5: ("tap", None),
    7: ("tap", None),
    9: ("tilt", 40.0),
    12: ("tap", None),
    17: ("tilt", 5.0),
    20: ("freefall", None),
    21: ("freefall", None),
    25: ("tilt", 45.0),
    26: ("tilt", 80.0),
    31: ("tap", None),
    38: ("tilt", 20.0),
    44: ("tap", None),
    47: ("tilt", 55.0),
}


def mag2(v):
    return v[0] * v[0] + v[1] * v[1] + v[2] * v[2]


def tilted(prev, cur):
    """The attitude test of rules_e19.c, multiplication only, no acos and no sqrt."""
    if prev is None:
        return False
    floor2 = (FREEFALL_G * G) * (FREEFALL_G * G)
    m_now, m_pr = mag2(cur), mag2(prev)
    if m_now < floor2 or m_pr < floor2:
        return False
    dot = cur[0] * prev[0] + cur[1] * prev[1] + cur[2] * prev[2]
    if dot <= 0.0:
        return True
    return dot * dot < (COS30 * COS30) * m_now * m_pr


def classify(prev, cur):
    """The four indices of rules_e19.c, in its order of tests."""
    m2 = mag2(cur)
    if m2 < (FREEFALL_G * G) * (FREEFALL_G * G):
        return 3
    if m2 > (TAP_G * G) * (TAP_G * G):
        return 1
    if tilted(prev, cur):
        return 2
    return 0


def stream(n, seed=SEED):
    """The fifty events, each rounded to four decimals as the CSV carries them."""
    rng = random.Random(seed)
    theta = 0.0
    rows = []
    for i in range(n):
        kind, value = SPEC.get(i, ("quiet", None))
        if kind == "tilt":
            theta = value
        if kind == "tap":
            size = rng.uniform(2.2, 3.4)
        elif kind == "freefall":
            size = rng.uniform(0.05, 0.25)
        else:
            size = 1.0
        rad = math.radians(theta)
        vec = (size * G * math.sin(rad), 0.0, size * G * math.cos(rad))
        vec = tuple(round(c + rng.gauss(0.0, 0.05), 4) for c in vec)
        rows.append((float(i), vec))
    return rows


def main():
    rows = stream(N)
    REPLAY.parent.mkdir(parents=True, exist_ok=True)
    ORACLE.parent.mkdir(parents=True, exist_ok=True)
    counts = {name: 0 for name in NAMES}
    armed = []
    blocked = []
    last_arm = None
    prev = None
    with REPLAY.open("w", newline="\n", encoding="ascii") as f_rep, \
            ORACLE.open("w", newline="\n", encoding="ascii") as f_orc:
        f_rep.write("t_s,tag,v0,v1,v2\n")
        f_orc.write("t_s,tag,rule,model\n")
        for t_s, vec in rows:
            idx = classify(prev, vec)
            name = NAMES[idx]
            counts[name] += 1
            arm = idx in (1, 3) and (last_arm is None or t_s - last_arm >= MIN_SEND_S)
            if arm:
                last_arm = t_s
                armed.append((t_s, name, math.sqrt(mag2(vec)) / G))
            elif idx in (1, 3):
                blocked.append((t_s, name))
            f_rep.write("%.1f,%s,%.4f,%.4f,%.4f\n" % (t_s, TAG, vec[0], vec[1], vec[2]))
            f_orc.write("%.6f,%s,%s,%s\n" % (t_s, TAG, name, STUB_TEXT if arm else '""'))
            prev = vec
    print("wrote %s and %s (seed %d)"
          % (REPLAY.relative_to(REPO).as_posix(), ORACLE.relative_to(REPO).as_posix(),
             SEED))
    for name in NAMES:
        print("%-9s %3d rows  %5.1f percent" % (name, counts[name], 100.0 * counts[name] / N))
    print("armed by the send rule: %d" % len(armed))
    for t_s, name, peak in armed:
        print("  t+%06.1f s  %-9s peak %.2f g" % (t_s, name, peak))
    print("blocked inside the minimum interval of %.0f s: %d" % (MIN_SEND_S, len(blocked)))
    for t_s, name in blocked:
        print("  t+%06.1f s  %s" % (t_s, name))
    print("the stub composes %d messages and the field checker refuses every one of them,"
          % len(armed))
    print("so the sends in CI are 0 of %d armed events; see tests/oracle/e19.expected.csv"
          % len(armed))


if __name__ == "__main__":
    main()
