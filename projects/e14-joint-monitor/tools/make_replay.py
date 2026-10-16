#!/usr/bin/env python3
"""Generate the e14 replay file and its oracle. Python is allowed here as a generator.

    python projects/e14-joint-monitor/tools/make_replay.py

Writes projects/e14-joint-monitor/replay/joint_100min.csv with the header
t_s,tag,v0,v1,v2, one row per minute for 100 minutes, where v0 is the CUMULATIVE CAN
error counter of the interface as /sys/class/net/<iface>/statistics reports it, v1 the
vibration peak of that minute in g (the largest magnitude of the acceleration vector
minus the 1 g the ADXL345 reads at rest), and v2 how many of the three rows of e13's
node table were PRESENT during the minute. It also writes tests/oracle/e14.expected.csv
with the header t_s,tag,rule,model, where rule is the decision of src/rules.rs applied
here in Python with the same threshold and the same precondition, and model is what the
stub answers for every row: the grammar's first alternative on every path with every
character class expanded to nothing, CSV quoted the way common/c/src/elog.c writes it.

The minutes are not a random walk. They are a written catalogue, because the point of
the file is that every branch of the rule and every pitfall of the chapter has a row:

  floods     a cansend flood at line rate, the cumulative counter rises
  taps       a hand tap on the joint, the peak goes above the threshold
  both       a flood and a tap in the same minute, which is the INSPECT case
  boundary   minute 31 has a flood and a peak of exactly 2.00 g, which is OK because
             the threshold is strict
  no node    minute 85 has a flood and a large peak with no node PRESENT, which is OK
             because there is nothing on the bus to inspect
  restart    minute 66 has a tap and a counter that went BACKWARDS, because the
             interface was taken down and up again; the rise is zero, not a wrap

Running the script twice writes the same bytes. The printed summary gives the count of
each decision; the OK fraction is the agreement the stub reaches in CI and the chapter
quotes it from this output.
"""
import random
from pathlib import Path

HERE = Path(__file__).resolve().parent
PROJECT = HERE.parent
REPO = PROJECT.parent.parent
REPLAY = PROJECT / "replay" / "joint_100min.csv"
ORACLE = REPO / "tests" / "oracle" / "e14.expected.csv"

N = 100
TAG = "minute"
MINUTE_S = 60.0
PEAK_G = 2.0                       # src/rules.rs, PEAK_G; strict
STUB_TEXT = '"{""decision"":""OK"",""summary"":"".""}"'
START_COUNTER = 48                 # the bus has run before this file starts

FLOODS = (7, 12, 13, 25, 31, 38, 44, 52, 57, 63, 71, 78, 85, 93)
TAPS = (3, 12, 19, 25, 31, 40, 44, 52, 57, 66, 71, 78, 85, 90, 93, 97)
BOUNDARY_MINUTE = 31               # a flood and a peak of exactly PEAK_G
NO_NODE_MINUTE = 85                # a flood, a large peak, and nothing on the bus
RESTART_MINUTE = 66                # the counter goes backwards after an ip link cycle
ONE_NODE_MINUTES = (21, 22, 23, 49, 50, 74)


def rows(seed=2026):
    """The hundred minutes, as (t_s, cumulative counter, peak in g, nodes present)."""
    rng = random.Random(seed)
    out = []
    counter = START_COUNTER
    for i in range(N):
        if i == RESTART_MINUTE:
            counter = 5           # the interface was brought down and up again
        elif i in FLOODS:
            counter += rng.randint(12, 180)
        if i == BOUNDARY_MINUTE:
            peak = PEAK_G
        elif i == NO_NODE_MINUTE:
            peak = round(rng.uniform(4.0, 9.0), 3)
        elif i in TAPS:
            peak = round(rng.uniform(2.4, 7.5), 3)
        else:
            peak = round(rng.uniform(0.02, 0.35), 3)
        if i == NO_NODE_MINUTE:
            nodes = 0
        elif i in ONE_NODE_MINUTES:
            nodes = 1
        else:
            nodes = 2
        out.append((i * MINUTE_S, counter, peak, nodes))
    return out


def rule(delta, peak, nodes):
    """src/rules.rs, decide(), in Python: the same three lines in the same order."""
    if nodes == 0:
        return "OK"
    if delta > 0 and peak > PEAK_G:
        return "INSPECT"
    return "OK"


def err_delta(previous, current):
    """src/rules.rs, err_delta(): a counter that went backwards is not a rise."""
    return max(0, current - previous)


def main():
    data = rows()
    REPLAY.parent.mkdir(parents=True, exist_ok=True)
    ORACLE.parent.mkdir(parents=True, exist_ok=True)
    counts = {"OK": 0, "INSPECT": 0}
    previous = None
    with REPLAY.open("w", newline="\n", encoding="ascii") as f_rep, \
            ORACLE.open("w", newline="\n", encoding="ascii") as f_orc:
        f_rep.write("t_s,tag,v0,v1,v2\n")
        f_orc.write("t_s,tag,rule,model\n")
        for t_s, counter, peak, nodes in data:
            delta = 0 if previous is None else err_delta(previous, counter)
            previous = counter
            verdict = rule(delta, peak, nodes)
            counts[verdict] += 1
            f_rep.write("%.1f,%s,%d,%.3f,%d\n" % (t_s, TAG, counter, peak, nodes))
            f_orc.write("%.6f,%s,%s,%s\n" % (t_s, TAG, verdict, STUB_TEXT))
    print("wrote %s and %s" % (REPLAY.relative_to(REPO).as_posix(),
                               ORACLE.relative_to(REPO).as_posix()))
    for key in ("OK", "INSPECT"):
        print("%-8s %3d minutes  %5.1f percent" % (key, counts[key], 100.0 * counts[key] / N))
    print("stub agreement (the OK fraction): %d of %d, %.1f percent"
          % (counts["OK"], N, 100.0 * counts["OK"] / N))
    print("floods %d, taps %d, both %d; the boundary minute, the no node minute and the "
          "restart minute are OK by construction"
          % (len(FLOODS), len(TAPS), len(set(FLOODS) & set(TAPS))))


if __name__ == "__main__":
    main()
