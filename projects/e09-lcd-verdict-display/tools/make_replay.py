#!/usr/bin/env python3
"""Generate the e09 replay file and its oracle. Python is allowed here as a generator.

    python projects/e09-lcd-verdict-display/tools/make_replay.py

Writes projects/e09-lcd-verdict-display/replay/adxl345_200.csv with the header
t_s,tag,v0,v1,v2 (x, y, z in g at 10 Hz, the order the IIO backend sorts the ADXL345's
in_accel_*_raw attributes) and tests/oracle/e09.expected.csv with the header
t_s,tag,rule,model, where rule is the decision of rules_e09.c applied in Python with
the same thresholds and the same 16-sample window, and model is what the CI stub
answers on the rows the loop asks the model about: the grammar's first expansion,
{"decision":"NONE"}, CSV-quoted the way common/c/src/elog.c writes it. On every other
row the model column is empty, because the e09 loop asks the model only when the
rule's verdict differs from the previous row's (src/main.cpp, --ask change), and the
logger writes an empty model cell for a row with no question.

The stream is seeded, so running the script twice writes the same bytes. It holds 25
cycles of TAP, TILT and FREEFALL, each event preceded by a rest segment. A tap and a
freefall each change the verdict twice (in, and back to NONE); a tilt changes it four
times, because the rule compares against the oldest reading of its window and so
reports the step into the tilt and, 1.6 seconds later, the tilted readings reaching
the oldest slot. The script counts the changes the rule actually makes and refuses to
write anything unless there are exactly 200, which are the 200 decisions the chapter's
acceptance test and agreement table count. The stub answers NONE on all 200 and agrees
on the 100 returns to NONE, so the CI agreement is 50.0 percent, printed below.
"""
import math
import random
from pathlib import Path

HERE = Path(__file__).resolve().parent
PROJECT = HERE.parent
REPO = PROJECT.parent.parent
REPLAY = PROJECT / "replay" / "adxl345_200.csv"
ORACLE = REPO / "tests" / "oracle" / "e09.expected.csv"
TAG = "adxl345"
RULE_HISTORY = 16                 # rules.h, the window the loop keeps for the rule
RATE_HZ = 10.0
CYCLES = 25
WANTED_CHANGES = 200
STUB_TEXT = '"{""decision"":""NONE""}"'   # the log's quoting of {"decision":"NONE"}
EMPTY_TEXT = '""'                           # elog.c's model cell for a row without a question


def rule(hist, v):
    """rules_e09.c, operation for operation: squares and a cosine, no libm."""
    x, y, z = v
    m2 = x * x + y * y + z * z
    if m2 > 4.0:
        return "TAP"
    if m2 < 0.09:
        return "FREEFALL"
    if not hist:
        return "NONE"
    rx, ry, rz = hist[0]
    r2 = rx * rx + ry * ry + rz * rz
    if r2 < 0.09:
        return "NONE"
    dot = x * rx + y * ry + z * rz
    if dot < 0.0 or dot * dot < 0.75 * m2 * r2:
        return "TILT"
    return "NONE"


def rest(rng):
    return (rng.gauss(0.0, 0.015), rng.gauss(0.0, 0.015), 1.0 + rng.gauss(0.0, 0.015))


def tilted(rng, theta_deg, phi_deg):
    t, p = math.radians(theta_deg), math.radians(phi_deg)
    return (math.sin(t) * math.cos(p) + rng.gauss(0.0, 0.015),
            math.sin(t) * math.sin(p) + rng.gauss(0.0, 0.015),
            math.cos(t) + rng.gauss(0.0, 0.015))


def stream(seed=2026):
    """Rows of (x, y, z) in g. Every value is rounded to three decimals before the rule
    reads it in Python and before it is written, so the C side parses the same double."""
    rng = random.Random(seed)
    rows = []
    for _ in range(CYCLES):
        for kind in ("TAP", "TILT", "FREEFALL"):
            for _ in range(rng.randint(20, 30)):
                rows.append(rest(rng))
            if kind == "TAP":
                rows.append((rng.uniform(-0.3, 0.3), rng.uniform(-0.3, 0.3),
                             1.0 + rng.uniform(1.6, 2.8)))
            elif kind == "TILT":
                theta, phi = rng.uniform(40.0, 70.0), rng.uniform(0.0, 360.0)
                for _ in range(rng.randint(8, 12)):
                    rows.append(tilted(rng, theta, phi))
            else:
                for _ in range(rng.randint(3, 5)):
                    rows.append((rng.uniform(-0.12, 0.12), rng.uniform(-0.12, 0.12),
                                 rng.uniform(-0.12, 0.12)))
    for _ in range(20):
        rows.append(rest(rng))
    return [tuple(round(c, 3) for c in r) for r in rows]


def main():
    rows = stream()
    hist = []
    prev = "NONE"                     # main.cpp starts with the previous verdict NONE
    decided = []                      # (t_s, rule, asked)
    counts = {"NONE": 0, "TAP": 0, "TILT": 0, "FREEFALL": 0}
    asked_counts = {"NONE": 0, "TAP": 0, "TILT": 0, "FREEFALL": 0}
    for i, v in enumerate(rows):
        r = rule(hist, v)
        asked = r != prev
        counts[r] += 1
        if asked:
            asked_counts[r] += 1
        decided.append((i / RATE_HZ, r, asked))
        hist.append(v)
        if len(hist) > RULE_HISTORY:
            hist.pop(0)
        prev = r
    changes = sum(1 for _, _, a in decided if a)
    if changes != WANTED_CHANGES:
        raise SystemExit(f"the rule changed its verdict {changes} times, not {WANTED_CHANGES};"
                         f" nothing written")
    REPLAY.parent.mkdir(parents=True, exist_ok=True)
    ORACLE.parent.mkdir(parents=True, exist_ok=True)
    with REPLAY.open("w", newline="\n", encoding="ascii") as f_rep, \
            ORACLE.open("w", newline="\n", encoding="ascii") as f_orc:
        f_rep.write("t_s,tag,v0,v1,v2\n")
        f_orc.write("t_s,tag,rule,model\n")
        for (t_s, r, asked), (x, y, z) in zip(decided, rows):
            f_rep.write(f"{t_s:.1f},{TAG},{x:.3f},{y:.3f},{z:.3f}\n")
            f_orc.write(f"{t_s:.6f},{TAG},{r},{STUB_TEXT if asked else EMPTY_TEXT}\n")
    print(f"wrote {REPLAY.relative_to(REPO).as_posix()} and {ORACLE.relative_to(REPO).as_posix()}")
    print(f"{len(rows)} rows at {RATE_HZ:.0f} Hz, {len(rows) / RATE_HZ:.1f} s of replay")
    for k, v in counts.items():
        print(f"{k:9s} {v:5d} rows   asked on {asked_counts[k]:4d}")
    agree = asked_counts["NONE"]
    print(f"decisions asked: {changes}; stub agreement (the NONE fraction of the asked rows):"
          f" {agree} of {changes}, {100.0 * agree / changes:.1f} percent")


if __name__ == "__main__":
    main()
