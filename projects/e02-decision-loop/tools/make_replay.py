#!/usr/bin/env python3
"""Generate the e02 replay file and its oracle. Python is allowed here as a generator.

    python projects/e02-decision-loop/tools/make_replay.py

Writes projects/e02-decision-loop/replay/synthetic_1000.csv with the header
t_s,tag,v0,v1 (v0 a level from 0 to 100, v1 its slope in units per second) and
tests/oracle/e02.expected.csv with the header t_s,tag,rule,model, where rule is the
decision of rules_e02.c applied in Python with the same two thresholds, and model is
what the CI stub answers for every row: the grammar's first expansion, the JSON object
{"decision":"NORMAL"}, CSV-quoted the way common/c/src/elog.c writes it, so that
tests/oracle_check compares the two files byte for byte on those two columns.

The walk is seeded, so running the script twice writes the same bytes. The level drifts
with small steps, surges upward every so often at a slope the rule must call ALERT, and
decays afterwards through the WATCH band, so all three decisions occur. The printed
summary gives the fraction of each decision; the NORMAL fraction is the agreement the
stub reaches in CI, and the chapter quotes it from this output.
"""
import random
from pathlib import Path

HERE = Path(__file__).resolve().parent
PROJECT = HERE.parent
REPO = PROJECT.parent.parent
REPLAY = PROJECT / "replay" / "synthetic_1000.csv"
ORACLE = REPO / "tests" / "oracle" / "e02.expected.csv"
N = 1000
TAG = "synthetic"
STUB_ANSWER = "NORMAL"
STUB_TEXT = '"{""decision"":""NORMAL""}"'      # the log's quoting of {"decision":"NORMAL"}


def rule(v0, v1):
    """The same two thresholds as rules_e02.c, in the same order."""
    if v0 > 80.0 or v1 > 10.0:
        return "ALERT"
    if v0 > 60.0:
        return "WATCH"
    return "NORMAL"


def walk(n, seed=2026):
    rng = random.Random(seed)
    level = 40.0
    surge_left = 0
    rows = []
    prev = level
    for i in range(n):
        if surge_left == 0 and rng.random() < 0.02:
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
    counts = {"NORMAL": 0, "WATCH": 0, "ALERT": 0}
    with REPLAY.open("w", newline="\n", encoding="ascii") as f_rep, \
            ORACLE.open("w", newline="\n", encoding="ascii") as f_orc:
        f_rep.write("t_s,tag,v0,v1\n")
        f_orc.write("t_s,tag,rule,model\n")
        for t_s, v0, v1 in rows:
            r = rule(v0, v1)
            counts[r] += 1
            f_rep.write(f"{t_s:.1f},{TAG},{v0:.2f},{v1:.2f}\n")
            f_orc.write(f"{t_s:.6f},{TAG},{r},{STUB_TEXT}\n")
    print(f"wrote {REPLAY.relative_to(REPO).as_posix()} and {ORACLE.relative_to(REPO).as_posix()}")
    for k, v in counts.items():
        print(f"{k:7s} {v:4d} rows  {100.0 * v / N:5.1f} percent")
    print(f"stub agreement (the {STUB_ANSWER} fraction): {counts[STUB_ANSWER]} of {N},"
          f" {100.0 * counts[STUB_ANSWER] / N:.1f} percent")


if __name__ == "__main__":
    main()
