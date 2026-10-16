#!/usr/bin/env python3
"""Generate the e04 replay file and its oracle. Python is allowed here as a generator.

    python projects/e04-thermal-narrator/tools/make_replay.py

Writes projects/e04-thermal-narrator/replay/benchmark_180.csv with the header
t_s,tag,v0,v1,v2: one row per minute for three hours, v0 the SoC temperature in
millidegrees as /sys/class/thermal/thermal_zone0/temp prints it, v1 the word vcgencmd
get_throttled prints as a decimal number, v2 the ARM clock in hertz from vcgencmd
measure_clock arm. And tests/oracle/e04.expected.csv with the header t_s,tag,rule,model,
where rule is the decision of rules_e04.c applied in Python with the same thresholds and
model is what the mock server answers for every row: the grammar's first expansion,
read from grammar.gbnf through mock_server.first_expansion() and CSV-quoted the way
common/c/src/elog.c writes it, so that tests/oracle_check compares the two files byte
for byte on those two columns.

The trace is a model of an e01 benchmark run on a Pi 4 Model B with no heatsink, and it
is drawn dashed in the chapter: idle near 46 C, a 90 minute run from minute 20 that
heats the SoC with a 12 minute time constant towards 84 C, the firmware's soft limit
at 80 C (bit 3, with bit 1 for the capped clock) pulling the clock from 1500 MHz to
1000 MHz and the temperature back under 80 C, so the state oscillates around the
threshold as it does on a real board, a cool-down, and a second 30 minute run from
minute 130. Bits 17 and 19 stay set once the limit has occurred, and never decide
anything. The walk is seeded, so running the script twice writes the same bytes. The
printed summary gives the fraction of each decision; the COOL fraction is the
agreement the mock reaches in CI, and the chapter quotes it from this output.
"""
import math
import random
import sys
from pathlib import Path

HERE = Path(__file__).resolve().parent
sys.path.insert(0, str(HERE))
from mock_server import first_expansion  # noqa: E402

PROJECT = HERE.parent
REPO = PROJECT.parent.parent
REPLAY = PROJECT / "replay" / "benchmark_180.csv"
ORACLE = REPO / "tests" / "oracle" / "e04.expected.csv"
GRAMMAR = PROJECT / "grammar.gbnf"
N = 180
TAG = "thermal_zone0/temp"
STUB_ANSWER = "COOL"

BIT_CAPPED = 0x2
BIT_THROTTLED = 0x4
BIT_SOFT_LIMIT = 0x8
BIT_CAPPED_OCCURRED = 0x20000
BIT_SOFT_LIMIT_OCCURRED = 0x80000
CLOCK_FULL = 1500000000
CLOCK_CAPPED = 1000000000


def rule(temp_mc, bits):
    """The same thresholds as rules_e04.c, in the same order."""
    temp_c = temp_mc / 1000.0
    if (bits & BIT_THROTTLED) or (bits & BIT_SOFT_LIMIT) or temp_c > 80.0:
        return "THROTTLED"
    if temp_c > 65.0:
        return "WARM"
    return "COOL"


def walk(n, seed=2026):
    rng = random.Random(seed)
    temp = 46.0
    occurred = 0
    rows = []
    for i in range(n):
        running = 20 <= i < 110 or 130 <= i < 160
        bits = occurred
        clock = CLOCK_FULL
        if running:
            target, tau = 84.0, 12.0
            if temp > 80.0:
                bits |= BIT_SOFT_LIMIT | BIT_CAPPED
                clock = CLOCK_CAPPED
                target, tau = 78.5, 6.0
                occurred |= BIT_SOFT_LIMIT_OCCURRED | BIT_CAPPED_OCCURRED
                bits |= occurred
        else:
            target, tau = 46.0, 10.0
        temp = target + (temp - target) * math.exp(-1.0 / tau) + rng.gauss(0.0, 0.3)
        temp = min(90.0, max(30.0, temp))
        rows.append((60.0 * i, int(round(temp * 1000.0)), bits, clock))
    return rows


def csv_quoted(text):
    return '"' + text.replace('"', '""') + '"'


def main():
    rows = walk(N)
    stub_text = csv_quoted(first_expansion(GRAMMAR.read_text(encoding="ascii")))
    REPLAY.parent.mkdir(parents=True, exist_ok=True)
    ORACLE.parent.mkdir(parents=True, exist_ok=True)
    counts = {"COOL": 0, "WARM": 0, "THROTTLED": 0}
    transitions = 0
    previous = None
    with REPLAY.open("w", newline="\n", encoding="ascii") as f_rep, \
            ORACLE.open("w", newline="\n", encoding="ascii") as f_orc:
        f_rep.write("t_s,tag,v0,v1,v2\n")
        f_orc.write("t_s,tag,rule,model\n")
        for t_s, temp_mc, bits, clock in rows:
            r = rule(temp_mc, bits)
            counts[r] += 1
            if previous is not None and r != previous:
                transitions += 1
            previous = r
            f_rep.write("%.1f,%s,%d,%d,%d\n" % (t_s, TAG, temp_mc, bits, clock))
            f_orc.write("%.6f,%s,%s,%s\n" % (t_s, TAG, r, stub_text))
    print("wrote %s and %s" % (REPLAY.relative_to(REPO).as_posix(),
                               ORACLE.relative_to(REPO).as_posix()))
    print("mock answer: %s" % stub_text)
    for k, v in counts.items():
        print("%-9s %4d rows  %5.1f percent" % (k, v, 100.0 * v / N))
    print("rows whose decision differs from the previous row: %d" % transitions)
    print("mock agreement (the %s fraction): %d of %d, %.1f percent"
          % (STUB_ANSWER, counts[STUB_ANSWER], N, 100.0 * counts[STUB_ANSWER] / N))


if __name__ == "__main__":
    main()
