#!/usr/bin/env python3
"""Generate the e03 replay file and its oracle. Python is allowed here as a generator.

    python projects/e03-led-explainer/tools/make_replay.py

Writes projects/e03-led-explainer/replay/state_200.csv with the header t_s,tag,v0,v1
(v0 a level from 0 to 100, v1 its slope in units per second: the state stream of e02,
200 rows, one per second) and tests/oracle/e03.expected.csv with the header
t_s,tag,rule,model, where rule is the colour rules_e03.c lights, applied in Python with
the same two thresholds, and model is what the CI stub answers for every row: the
grammar's first expansion with every character class empty, {"colour":"GREEN","reason":""},
CSV-quoted the way common/c/src/elog.c writes it, so that tests/oracle_check compares
the two files byte for byte on those two columns.

The walk is the same shape as e02's and seeded, so running the script twice writes the
same bytes. The level drifts with small steps, surges upward every so often at a slope
the rule must call RED, and decays afterwards through the YELLOW band, so all three
colours occur and RED occurs by slope alone as well as by level. The printed summary
gives the count of each colour and of each RED cause; the GREEN fraction is the colour
agreement the stub reaches in CI, and the reason fraction in CI is zero by construction,
because the stub's reason is empty. The chapter quotes both from this output.
"""
import random
from pathlib import Path

HERE = Path(__file__).resolve().parent
PROJECT = HERE.parent
REPO = PROJECT.parent.parent
REPLAY = PROJECT / "replay" / "state_200.csv"
ORACLE = REPO / "tests" / "oracle" / "e03.expected.csv"
N = 200
SEED = 2026
TAG = "state"
STUB_COLOUR = "GREEN"
STUB_TEXT = '"{""colour"":""GREEN"",""reason"":""""}"'   # the log's quoting of the stub


def rule(v0, v1):
    """The same two thresholds as rules_e03.c, in the same order."""
    if v0 > 80.0 or v1 > 10.0:
        return "RED"
    if v0 > 60.0:
        return "YELLOW"
    return "GREEN"


def cause(v0, v1):
    """Which field the rule triggered on, the set reason_check.py accepts."""
    if v0 > 80.0 and v1 > 10.0:
        return "level or slope"
    if v1 > 10.0:
        return "slope"
    return "level"


def walk(n, seed=SEED):
    rng = random.Random(seed)
    level = 40.0
    surge_left = 0
    rows = []
    prev = level
    for i in range(n):
        if surge_left == 0 and rng.random() < 0.03:
            surge_left = rng.randint(2, 4)
        if surge_left:
            level += rng.uniform(11.0, 16.0)
            surge_left -= 1
        elif level > 60.0:
            level -= rng.uniform(1.0, 4.0)
        else:
            level += rng.gauss(0.0, 2.5)
        level = min(100.0, max(0.0, level))
        v0 = round(level, 2)
        v1 = round(v0 - prev, 2) if i else 0.0
        prev = v0
        rows.append((float(i), v0, v1))
    return rows


def main():
    rows = walk(N)
    REPLAY.parent.mkdir(parents=True, exist_ok=True)
    ORACLE.parent.mkdir(parents=True, exist_ok=True)
    counts = {"GREEN": 0, "YELLOW": 0, "RED": 0}
    causes = {"level": 0, "slope": 0, "level or slope": 0}
    with REPLAY.open("w", newline="\n", encoding="ascii") as f_rep, \
            ORACLE.open("w", newline="\n", encoding="ascii") as f_orc:
        f_rep.write("t_s,tag,v0,v1\n")
        f_orc.write("t_s,tag,rule,model\n")
        for t_s, v0, v1 in rows:
            r = rule(v0, v1)
            counts[r] += 1
            if r == "RED":
                causes[cause(v0, v1)] += 1
            f_rep.write(f"{t_s:.1f},{TAG},{v0:.2f},{v1:.2f}\n")
            f_orc.write(f"{t_s:.6f},{TAG},{r},{STUB_TEXT}\n")
    print(f"wrote {REPLAY.relative_to(REPO).as_posix()} and "
          f"{ORACLE.relative_to(REPO).as_posix()} (seed {SEED})")
    for k, v in counts.items():
        print(f"{k:7s} {v:4d} rows  {100.0 * v / N:5.1f} percent")
    print("RED by level only {level}, by slope only {slope}, by both {both}".format(
        level=causes["level"], slope=causes["slope"], both=causes["level or slope"]))
    print(f"stub colour agreement (the {STUB_COLOUR} fraction): {counts[STUB_COLOUR]} of {N},"
          f" {100.0 * counts[STUB_COLOUR] / N:.1f} percent; stub reason fraction: 0 of {N}")


if __name__ == "__main__":
    main()
