#!/usr/bin/env python3
"""Generate the e15 replay file and its oracle. Python is this project's language.

    python projects/e15-esp32-sidecar/tools/make_replay.py

Writes projects/e15-esp32-sidecar/replay/budgets_200.csv with the header
t_s,tag,v0,v1,v2,v3,v4,v5 (v0 the budget in milliamps, v1 the minimum on-time in
percent, v2 to v5 the mean cost of PATTERN_A to PATTERN_D in milliamps above the
LED-off baseline) and tests/oracle/e15.expected.csv with the header t_s,tag,rule,model,
where rule is the decision of edgellm_e15/rules.py applied here with the same on-time
table, and model is what the CI stub answers for every row: the grammar's first
expansion, the JSON object {"decision":"PATTERN_A"}, CSV-quoted the way the logger
writes it, so that tests/test_oracle.py compares the two files on those two columns.

The costs come from evidence/pattern_costs_summary.csv when the PPK2 run has written
it (tools/measure_patterns.py, columns pattern,mean_ma,spread_pct,runs); until then
they are the placeholder below, derived from one named record: the red LK-LED10 at
3.3 V draws 6.34 mA (PPK2, Saturday 3 October 2026, the bench record), and the
pattern's on-time scales it. The generator prints which it used, and the chapter says
the placeholder is a derivation, not a measurement. The draw is seeded, so running the
script twice writes the same bytes. The printed summary gives the fraction of each
decision; the PATTERN_A fraction is the agreement the stub reaches in CI.
"""
import csv
import random
from pathlib import Path

HERE = Path(__file__).resolve().parent
PROJECT = HERE.parent
REPO = PROJECT.parent.parent
REPLAY = PROJECT / "replay" / "budgets_200.csv"
ORACLE = REPO / "tests" / "oracle" / "e15.expected.csv"
EVIDENCE = PROJECT / "evidence" / "pattern_costs_summary.csv"
N = 200
TAG = "budget"
NAMES = ("PATTERN_A", "PATTERN_B", "PATTERN_C", "PATTERN_D")
ON_PCT = (100.0, 50.0, 20.0, 50.0)
RED_LED_MA = 6.34                      # the red LK-LED10 at 3.3 V, PPK2, Saturday 3 October 2026
PLACEHOLDER = tuple(round(RED_LED_MA * p / 100.0, 2) for p in ON_PCT)
STUB_ANSWER = "PATTERN_A"
STUB_TEXT = '"{""decision"":""PATTERN_A""}"'   # the log's quoting of {"decision":"PATTERN_A"}
ON_CHOICES = (10, 20, 25, 40, 50, 60, 75, 100)
ON_WEIGHTS = (10, 15, 12, 13, 15, 12, 13, 10)


def rule(ev):
    """The same choice as edgellm_e15/rules.py: cheapest pattern meeting the on-time."""
    best, best_cost = 0, None
    for i in range(4):
        if ON_PCT[i] < ev[1]:
            continue
        if best_cost is None or ev[2 + i] < best_cost:
            best, best_cost = i, ev[2 + i]
    return NAMES[best]


def costs():
    """Measured means from evidence/, else the placeholder; returns (costs, source)."""
    if EVIDENCE.exists():
        by_name = {}
        with EVIDENCE.open(newline="", encoding="ascii") as fh:
            for row in csv.DictReader(fh):
                by_name[row["pattern"]] = round(float(row["mean_ma"]), 2)
        if all(n in by_name for n in NAMES):
            return tuple(by_name[n] for n in NAMES), "measured, " + EVIDENCE.name
    return PLACEHOLDER, "placeholder: red LK-LED10 6.34 mA scaled by on-time"


def draw(n, cost, seed=2026):
    rng = random.Random(seed)
    rows = []
    for i in range(n):
        budget = round(rng.uniform(0.5, 8.0), 1)
        on_pct = rng.choices(ON_CHOICES, ON_WEIGHTS)[0]
        rows.append((float(i), budget, float(on_pct)) + cost)
    return rows


def main():
    cost, source = costs()
    rows = draw(N, cost)
    REPLAY.parent.mkdir(parents=True, exist_ok=True)
    ORACLE.parent.mkdir(parents=True, exist_ok=True)
    counts = {n: 0 for n in NAMES}
    with REPLAY.open("w", newline="\n", encoding="ascii") as f_rep, \
            ORACLE.open("w", newline="\n", encoding="ascii") as f_orc:
        f_rep.write("t_s,tag,v0,v1,v2,v3,v4,v5\n")
        f_orc.write("t_s,tag,rule,model\n")
        for row in rows:
            r = rule(row[1:])
            counts[r] += 1
            f_rep.write("%.1f,%s,%.1f,%.0f,%.2f,%.2f,%.2f,%.2f\n" % ((row[0], TAG) + row[1:]))
            f_orc.write("%.6f,%s,%s,%s\n" % (row[0], TAG, r, STUB_TEXT))
    print("wrote %s and %s" % (REPLAY.relative_to(REPO).as_posix(),
                               ORACLE.relative_to(REPO).as_posix()))
    print("costs mA above baseline, A to D: %s (%s)" % (", ".join("%.2f" % c for c in cost),
                                                       source))
    for k, v in counts.items():
        print("%-10s %4d rows  %5.1f percent" % (k, v, 100.0 * v / N))
    print("stub agreement (the %s fraction): %d of %d, %.1f percent"
          % (STUB_ANSWER, counts[STUB_ANSWER], N, 100.0 * counts[STUB_ANSWER] / N))


if __name__ == "__main__":
    main()
