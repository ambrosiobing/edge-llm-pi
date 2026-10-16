#!/usr/bin/env python3
"""Generate the e18 register stream and its oracle. Python is allowed here as a
generator, never as the project's language.

    python projects/e18-industrial-logger/tools/make_replay.py

Writes projects/e18-industrial-logger/replay/registers_600.csv with the header
t_s,tag,v0,v1,v2,v3, one row per polled frame, and
tests/oracle/e18.expected.csv with the header t_s,tag,rule,model.

The four values are the register block the logger reads from the peer, already in
engineering counts, and the sequence number the frame carried:

    v0  register 0, level, counts, declared range 0 to 1000
    v1  register 1, temperature, tenths of a degree Celsius, range 0 to 1200
    v2  register 2, pressure, tenths of a bar, range 0 to 250
    v3  the frame's sequence number, 0 to 255, wrapping

The service re-encodes every replayed row into the 64 byte frame of
rust/src/registers.rs and decodes it again before the rule sees it, so the byte
offsets, the big-endian register order and the CRC are on the oracle path even
with no wire and no peer. What the replay stands in for is the peer, which does
not exist on this bench; nothing here is a measurement.

The stream is a seeded walk: a slow upward drift that must never reach the rule's
threshold, and two step changes that must. The printed summary gives the count of
each decision; the NOMINAL fraction is the agreement the stub reaches in CI, and
the chapter quotes it from this output.
"""
import random
from pathlib import Path

HERE = Path(__file__).resolve().parent
PROJECT = HERE.parent
REPO = PROJECT.parent.parent
REPLAY = PROJECT / "replay" / "registers_600.csv"
ORACLE = REPO / "tests" / "oracle" / "e18.expected.csv"

N = 600                 # frames
PERIOD_S = 2.0          # the logger polls the peer every two seconds
TAG = "rs485a"

# The rule's three constants, the same three as rust/src/rules.rs. A change here
# without the same change there is caught by the oracle test on the next run.
REG_RANGE = 1000.0      # register 0 is declared 0 to 1000 counts
DRIFT_FRACTION = 0.10   # a span above a tenth of the range is DRIFT
WINDOW = 30             # frames, the current one and the 29 before it

NAMES = ("NOMINAL", "DRIFT")
STUB_ANSWER = "NOMINAL"
# The stub expands the grammar's first alternative on every path and a character
# class to nothing, so the answer is {"decision":"NOMINAL","summary":"."}, written
# here with the CSV quoting of common/c/src/elog.c and of the Rust logger.
STUB_TEXT = '"{""decision"":""NOMINAL"",""summary"":"".""}"'


def rule(window):
    """The same span test as rust/src/rules.rs, on the same window order.

    window holds the level of the current frame and of the frames before it, at
    most WINDOW values, oldest first. A span strictly above the fraction of the
    declared range is DRIFT; the threshold value itself stays NOMINAL.
    """
    if not window:
        return 0
    span = max(window) - min(window)
    return 1 if span > DRIFT_FRACTION * REG_RANGE else 0


def walk(n, seed=2026):
    """The level, the temperature, the pressure and the sequence number per frame.

    The level drifts upward by 0.08 counts per frame with a small noise, which over
    any window of 30 frames spans a few counts and never the 100 the rule asks for.
    Two step changes do reach it: plus 140 counts at frame 180 and minus 170 at
    frame 400. Each step stays inside the window for WINDOW frames, which is how
    long the rule says DRIFT.
    """
    rng = random.Random(seed)
    level = 420.0
    temp = 418.0
    press = 61.0
    rows = []
    for i in range(n):
        level += 0.08 + rng.gauss(0.0, 1.2)
        if i == 180:
            level += 140.0
        if i == 400:
            level -= 170.0
        temp += rng.gauss(0.0, 0.7)
        press += rng.gauss(0.0, 0.35)
        level = min(1000.0, max(0.0, level))
        temp = min(1200.0, max(0.0, temp))
        press = min(250.0, max(0.0, press))
        rows.append((i * PERIOD_S, round(level), round(temp), round(press), i % 256))
    return rows


def main():
    rows = walk(N)
    REPLAY.parent.mkdir(parents=True, exist_ok=True)
    ORACLE.parent.mkdir(parents=True, exist_ok=True)
    counts = {name: 0 for name in NAMES}
    window = []
    with REPLAY.open("w", newline="\n", encoding="ascii") as f_rep, \
            ORACLE.open("w", newline="\n", encoding="ascii") as f_orc:
        f_rep.write("t_s,tag,v0,v1,v2,v3\n")
        f_orc.write("t_s,tag,rule,model\n")
        for t_s, level, temp, press, seq in rows:
            window.append(float(level))
            if len(window) > WINDOW:
                window.pop(0)
            name = NAMES[rule(window)]
            counts[name] += 1
            f_rep.write(f"{t_s:.1f},{TAG},{level},{temp},{press},{seq}\n")
            f_orc.write(f"{t_s:.6f},{TAG},{name},{STUB_TEXT}\n")
    print(f"wrote {REPLAY.relative_to(REPO).as_posix()} and {ORACLE.relative_to(REPO).as_posix()}")
    print(f"window {WINDOW} frames, range {REG_RANGE:.0f} counts, "
          f"threshold {DRIFT_FRACTION * REG_RANGE:.0f} counts")
    for name in NAMES:
        print(f"{name:8s} {counts[name]:4d} rows  {100.0 * counts[name] / N:5.1f} percent")
    print(f"stub agreement (the {STUB_ANSWER} fraction): {counts[STUB_ANSWER]} of {N},"
          f" {100.0 * counts[STUB_ANSWER] / N:.1f} percent")


if __name__ == "__main__":
    main()
