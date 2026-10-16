#!/usr/bin/env python3
"""Generate the e16 scenario file and its oracle. Python is allowed here as a generator.

    python projects/e16-nbiot-sleep-interval/tools/make_replay.py

Writes projects/e16-nbiot-sleep-interval/replay/profiles_100.csv with the header
t_s,tag,v0,v1,v2,v3,v4,v5: one row per scenario, where v0 is the sleep floor of the
module and its board in microamps, v1 the charge of one wake window in microamp hours,
v2 the length of that wake in seconds, v3 the cell in milliamp hours, v4 the battery
target in days and v5 the power-saving-mode window the network granted in seconds. And
tests/oracle/e16.expected.csv with the header t_s,tag,rule,model, where rule is the
decision of rules_e16.c applied in Python with the same arithmetic as src/battery.c and
model is what the CI stub answers for every row: the grammar's first expansion with
every character class empty, {"decision":"I60","reason":"."}, CSV-quoted the way
common/c/src/elog.c writes it, so that tests/oracle_check compares the two files byte
for byte on those two columns.

The scenarios are modelled and the chapter draws them dashed. Only the shape of a
profile is taken from the bench: a floor in the tens of microamps, a wake of tens of
microamp hours over tens of seconds, and a granted window that is the network's choice
and not the module's request. The granted windows are six values the 3GPP timer octets
of src/at_cycle.c can carry around the four intervals, with 0 standing for the case the
chapter cares about most: a reply whose unit is not in the application note's table,
which is read as no window granted at all.

The walk is seeded, so running the script twice writes the same bytes, and the rule is
applied to the rounded values that are actually written, so the Python oracle and the C
rule see the same numbers. The printed summary gives the count of each decision; the
I60 fraction is the agreement the stub reaches in CI, and the chapter quotes it from
this output.
"""
import random
from pathlib import Path

HERE = Path(__file__).resolve().parent
PROJECT = HERE.parent
REPO = PROJECT.parent.parent
REPLAY = PROJECT / "replay" / "profiles_100.csv"
ORACLE = REPO / "tests" / "oracle" / "e16.expected.csv"
N = 100
SEED = 2026
TAG = "ppk2_cycle"
STUB_DECISION = "I60"
STUB_TEXT = '"{""decision"":""I60"",""reason"":"".""}"'   # the log's quoting of the stub

INTERVALS = (60.0, 300.0, 900.0, 3600.0)
NAMES = ("I60", "I300", "I900", "I3600")
# The windows a network can grant, in seconds, each one a 3GPP octet src/at_cycle.c can
# decode: unit 5 (1 min) times 5 and times 30, unit 0 (10 min) times 1, unit 1 (1 h)
# times 1, unit 2 (10 h) times 1. The 0 is the case the chapter cares about most: a
# reply whose unit is not in the application note's table, read as no window granted.
GRANTS = (0.0, 300.0, 600.0, 1800.0, 3600.0, 36000.0)
CAPACITIES = (1200.0, 2000.0, 3000.0)
# Targets that can discriminate between the four intervals for profiles of this shape:
# the reference profile of src/battery_test.c lives 28 days at 60 s, 137 at 300 s, 397
# at 900 s and 1390 at 3600 s, so a target of several years would answer I60 everywhere
# and measure nothing.
TARGETS = (60.0, 120.0, 180.0, 365.0, 730.0)


def charge_per_cycle_uah(sleep_ua, active_uah, active_s, interval_s):
    """The same arithmetic as battery_charge_per_cycle_uah() in src/battery.c."""
    sleep_s = max(interval_s - active_s, 0.0)
    return active_uah + sleep_ua * sleep_s / 3600.0


def life_days(sleep_ua, active_uah, active_s, capacity_mah, interval_s):
    """The same arithmetic as battery_life_days() in src/battery.c."""
    per_day = charge_per_cycle_uah(sleep_ua, active_uah, active_s, interval_s) * \
        86400.0 / interval_s
    return capacity_mah * 1000.0 / per_day if per_day > 0.0 else 0.0


def rule(sleep_ua, active_uah, active_s, capacity_mah, target_days, granted_s):
    """The same search as rules_e16.c: the largest interval inside the granted window
    whose life meets the target, else I60, the safe fallback."""
    if active_uah <= active_s * sleep_ua / 3600.0:        # battery_valid_profile()
        return NAMES[0]
    for i in range(len(INTERVALS) - 1, 0, -1):
        if INTERVALS[i] <= granted_s and \
                life_days(sleep_ua, active_uah, active_s, capacity_mah, INTERVALS[i]) >= \
                target_days:
            return NAMES[i]
    return NAMES[0]


def scenarios(n, seed=SEED):
    rng = random.Random(seed)
    rows = []
    for i in range(n):
        sleep_ua = round(rng.uniform(8.0, 60.0), 1)
        active_s = round(rng.uniform(8.0, 30.0), 1)
        # The wake must cost more charge than the same span at the floor, which is what
        # battery_valid_profile() demands and what a radio always does; the lower bound
        # below is that span plus a margin, so no scenario is refused by accident.
        floor_uah = active_s * sleep_ua / 3600.0
        active_uah = round(max(rng.uniform(20.0, 120.0), floor_uah + 5.0), 1)
        capacity_mah = rng.choice(CAPACITIES)
        target_days = rng.choice(TARGETS)
        granted_s = rng.choice(GRANTS)
        rows.append((float(i), sleep_ua, active_uah, active_s, capacity_mah,
                     target_days, granted_s))
    return rows


def main():
    rows = scenarios(N)
    REPLAY.parent.mkdir(parents=True, exist_ok=True)
    ORACLE.parent.mkdir(parents=True, exist_ok=True)
    counts = dict((name, 0) for name in NAMES)
    bounded = 0
    with REPLAY.open("w", newline="\n", encoding="ascii") as f_rep, \
            ORACLE.open("w", newline="\n", encoding="ascii") as f_orc:
        f_rep.write("t_s,tag,v0,v1,v2,v3,v4,v5\n")
        f_orc.write("t_s,tag,rule,model\n")
        for t_s, sleep_ua, active_uah, active_s, cap, target, granted in rows:
            decision = rule(sleep_ua, active_uah, active_s, cap, target, granted)
            counts[decision] += 1
            if decision == NAMES[0] and granted >= INTERVALS[1]:
                bounded += 1          # the target, not the grant, held it at I60
            f_rep.write("%.1f,%s,%.1f,%.1f,%.1f,%.0f,%.0f,%.0f\n" %
                        (t_s, TAG, sleep_ua, active_uah, active_s, cap, target, granted))
            f_orc.write("%.6f,%s,%s,%s\n" % (t_s, TAG, decision, STUB_TEXT))
    print("wrote %s and %s (seed %d)" % (REPLAY.relative_to(REPO).as_posix(),
                                         ORACLE.relative_to(REPO).as_posix(), SEED))
    for name in NAMES:
        print("%-6s %4d rows  %5.1f percent" % (name, counts[name],
                                                100.0 * counts[name] / N))
    print("of the %d I60 rows, %d were held there by the battery target and the rest by "
          "the granted window" % (counts[NAMES[0]], bounded))
    print("stub agreement (the %s fraction): %d of %d, %.1f percent" %
          (STUB_DECISION, counts[STUB_DECISION], N, 100.0 * counts[STUB_DECISION] / N))


if __name__ == "__main__":
    main()
