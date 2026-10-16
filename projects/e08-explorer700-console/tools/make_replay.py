#!/usr/bin/env python3
"""Generate the e08 replay file and its oracle. Python is allowed here as a generator.

    python projects/e08-explorer700-console/tools/make_replay.py

Writes projects/e08-explorer700-console/replay/console_120.csv with the header
t_s,tag,v0,v1,v2,v3,v4: one row per joystick press, v0 the question index 0 to 4, v1
the DS3231's seconds since the epoch, v2 the Pi's seconds since the epoch at the same
moment (the clock pair), v3 the BMP280 pressure in kPa and v4 its temperature in C
(the sensor pair). And tests/oracle/e08.expected.csv with the header t_s,tag,rule,model,
where rule is the decision of rules_e08.c applied in Python with the same two
thresholds on the same difference, and model is what the CI stub answers for a
sentence grammar: the class expands to nothing, so the object is {"answer":"."},
CSV-quoted the way common/c/src/elog.c writes it.

The walk is seeded, so running the script twice writes the same bytes. The Pi clock
advances by the gap between presses; the DS3231 follows it with a small offset most of
the time, drifts away over a few presses twice (the DRIFTING band, 2 to 60 s), and is
set wrong by more than a minute in two episodes (LOST), once ahead and once behind, so
that the rule's three states and the buzzer's transition into LOST all occur. The
pressure is a slow random walk around 98 kPa and the temperature around 24 C. The
printed summary gives the count of each state and the number of transitions into LOST,
which is the number of times the buzzer sounds over the replay; the chapter quotes it.
"""
import random
from pathlib import Path

HERE = Path(__file__).resolve().parent
PROJECT = HERE.parent
REPO = PROJECT.parent.parent
REPLAY = PROJECT / "replay" / "console_120.csv"
ORACLE = REPO / "tests" / "oracle" / "e08.expected.csv"
N = 120
TAG = "explorer700"
T0 = 1760440000.0                       # Tuesday 14 October 2025, 11:06:40 UTC, a fixed epoch
STUB_TEXT = '"{""answer"":"".""}"'       # the log's quoting of {"answer":"."}


def rule(rtc_s, pi_s):
    """The same two thresholds as rules_e08.c, on the same difference, in the same order."""
    d = abs(rtc_s - pi_s)
    if d > 60.0:
        return "LOST"
    if d > 2.0:
        return "DRIFTING"
    return "IN_SYNC"


def walk(n, seed=2026):
    rng = random.Random(seed)
    pi_s = T0
    offset = 0.4                        # DS3231 minus Pi, seconds
    pressure = 98.20
    temperature = 24.0
    rows = []
    # Episodes by press index: (start, length, kind). Chosen so that the IN_SYNC band
    # holds most presses and the two LOST episodes are separated by a return to sync.
    episodes = {}
    for start, length, kind in ((18, 6, "drift"), (40, 5, "lost_ahead"), (70, 8, "drift"),
                                (96, 4, "lost_behind")):
        for k in range(length):
            episodes[start + k] = (kind, k)
    for i in range(n):
        gap = rng.uniform(20.0, 90.0)   # seconds between presses
        pi_s += gap
        kind, k = episodes.get(i, ("sync", 0))
        if kind == "sync":
            offset = rng.uniform(-1.5, 1.5)
        elif kind == "drift":
            offset = 3.0 + 7.0 * k + rng.uniform(0.0, 2.0)
        elif kind == "lost_ahead":
            offset = 120.0 + rng.uniform(0.0, 30.0)
        else:
            offset = -(90.0 + rng.uniform(0.0, 30.0))
        rtc_s = pi_s + offset
        pressure = min(104.0, max(94.0, pressure + rng.gauss(0.0, 0.06)))
        temperature = min(32.0, max(16.0, temperature + rng.gauss(0.0, 0.15)))
        q = rng.randrange(5)
        rows.append((round(pi_s - T0, 1), q, rtc_s, pi_s, round(pressure, 2),
                     round(temperature, 1)))
    return rows


def main():
    rows = walk(N)
    REPLAY.parent.mkdir(parents=True, exist_ok=True)
    ORACLE.parent.mkdir(parents=True, exist_ok=True)
    counts = {"IN_SYNC": 0, "DRIFTING": 0, "LOST": 0}
    questions = [0] * 5
    into_lost = 0
    last = None
    with REPLAY.open("w", newline="\n", encoding="ascii") as f_rep, \
            ORACLE.open("w", newline="\n", encoding="ascii") as f_orc:
        f_rep.write("t_s,tag,v0,v1,v2,v3,v4\n")
        f_orc.write("t_s,tag,rule,model\n")
        for t_s, q, rtc_s, pi_s, p_kpa, t_c in rows:
            r = rule(rtc_s, pi_s)
            counts[r] += 1
            questions[q] += 1
            if r == "LOST" and last != "LOST":
                into_lost += 1
            last = r
            f_rep.write(f"{t_s:.1f},{TAG},{q},{rtc_s:.2f},{pi_s:.2f},{p_kpa:.2f},{t_c:.1f}\n")
            f_orc.write(f"{t_s:.6f},{TAG},{r},{STUB_TEXT}\n")
    print(f"wrote {REPLAY.relative_to(REPO).as_posix()} and {ORACLE.relative_to(REPO).as_posix()}")
    for k, v in counts.items():
        print(f"{k:9s} {v:4d} rows  {100.0 * v / N:5.1f} percent")
    print("questions asked, index 0 to 4: " + ", ".join(str(c) for c in questions))
    print(f"transitions into LOST, the buzzer count over the replay: {into_lost}")
    print("stub answer in every row: {\"answer\":\".\"}, which carries no reading, so the")
    print("CI answer check scores 0 of %d by construction; the oracle test checks the shape" % N)


if __name__ == "__main__":
    main()
