#!/usr/bin/env python3
"""Generate the e06 replay file and its oracle. Python is allowed here as a generator.

    python projects/e06-vl53l4cd-too-close/tools/make_replay.py

Writes projects/e06-vl53l4cd-too-close/replay/approaches.csv with the header t_s,tag,v0,v1
(v0 the distance in mm, v1 the VL53L4CD range status, 0 for a valid range) at 10 samples
per second, and tests/oracle/e06.expected.csv with the header t_s,tag,rule,model, where
rule is the decision of rules_e06.c applied in Python with the same two conditions over
the same history, and model is what the CI stub answers for every row: the grammar's
first expansion, {"decision":"CLEAR","beam":"."}, CSV-quoted the way common/c/src/elog.c
writes it, so that tests/oracle_check compares the two files byte for byte on those two
columns.

The sequence is seeded, so running the script twice writes the same bytes. It is built
from the shapes the bench run of Thursday 29 October 2026 will contain: a hold at about
900 mm with noise, then an approach by hand at one of three speeds (slow 50 mm/s, medium
200 mm/s, fast 500 mm/s) down to a stop between 120 and 260 mm, a hold there, a retreat,
and a hold again. During fast approaches a few samples carry range status 2 or 4 (no
distance), as the sensor does when the signal is weak, and one approach in ten stops
short of 300 mm so that APPROACHING occurs with no TOO_CLOSE after it. The printed
summary gives the fraction of each decision; the CLEAR fraction is the agreement the stub
reaches in CI, and the chapter quotes it from this output.
"""
import random
from pathlib import Path

HERE = Path(__file__).resolve().parent
PROJECT = HERE.parent
REPO = PROJECT.parent.parent
REPLAY = PROJECT / "replay" / "approaches.csv"
ORACLE = REPO / "tests" / "oracle" / "e06.expected.csv"
TAG = "vl53l4cd"
DT = 0.1                                   # 10 samples per second, the sensor's interval
NOISE_MM = 2.0                             # the shape expected from the repeatability test
SPEEDS = {"slow": 50.0, "medium": 200.0, "fast": 500.0}
APPROACHES = ["slow", "medium", "fast"] * 7       # 21 approaches, 7 at each speed
STUB_ANSWER = "CLEAR"
STUB_TEXT = '"{""decision"":""CLEAR"",""beam"":"".""}"'  # the log's quoting of the stub


def is_range(sample):
    return sample[1] == 0


def rule(hist, sample):
    """rules_e06.c in Python: the same two conditions over the same history."""
    if not is_range(sample):
        return "CLEAR"
    if sample[0] < 300.0 and hist and is_range(hist[-1]) and hist[-1][0] < 300.0:
        return "TOO_CLOSE"
    if len(hist) < 4:
        return "CLEAR"
    seq = hist[-4:] + [sample]
    for a, b in zip(seq, seq[1:]):
        if not is_range(a) or not (b[0] < a[0]):
            return "CLEAR"
    return "APPROACHING"


def sequence(seed=2026):
    rng = random.Random(seed)
    rows = []

    def hold(mm, seconds):
        for _ in range(int(seconds / DT)):
            rows.append((int(round(mm + rng.gauss(0.0, NOISE_MM))), 0))

    def move(start, end, speed, status_dropouts):
        n = max(2, int(abs(end - start) / speed / DT))
        for i in range(1, n + 1):
            mm = start + (end - start) * i / n + rng.gauss(0.0, NOISE_MM)
            status = 0
            if status_dropouts and rng.random() < 0.08:
                status = rng.choice([2, 4])
            rows.append((int(round(max(mm, 1.0))), status))

    hold(900.0, 3.0)
    for k, name in enumerate(APPROACHES):
        speed = SPEEDS[name]
        far = 900.0 + rng.uniform(-30.0, 30.0)
        stop = rng.uniform(120.0, 260.0) if k % 10 != 9 else rng.uniform(330.0, 420.0)
        move(far, stop, speed, name == "fast")
        hold(stop, rng.uniform(1.0, 2.5))
        move(stop, far, speed * 2.0, False)
        hold(far, rng.uniform(1.5, 3.0))
    return rows


def main():
    rows = sequence()
    REPLAY.parent.mkdir(parents=True, exist_ok=True)
    ORACLE.parent.mkdir(parents=True, exist_ok=True)
    counts = {"CLEAR": 0, "APPROACHING": 0, "TOO_CLOSE": 0}
    hist = []
    n_bad = 0
    with REPLAY.open("w", newline="\n", encoding="ascii") as f_rep, \
            ORACLE.open("w", newline="\n", encoding="ascii") as f_orc:
        f_rep.write("t_s,tag,v0,v1\n")
        f_orc.write("t_s,tag,rule,model\n")
        for i, sample in enumerate(rows):
            t_s = i * DT
            r = rule(hist, sample)
            counts[r] += 1
            n_bad += 0 if is_range(sample) else 1
            f_rep.write(f"{t_s:.1f},{TAG},{sample[0]},{sample[1]}\n")
            f_orc.write(f"{t_s:.6f},{TAG},{r},{STUB_TEXT}\n")
            hist.append(sample)
            hist = hist[-16:]
    n = len(rows)
    print(f"wrote {REPLAY.relative_to(REPO).as_posix()} and {ORACLE.relative_to(REPO).as_posix()}")
    print(f"{n} rows at {DT} s, {len(APPROACHES)} approaches, {n_bad} samples with no range")
    for k, v in counts.items():
        print(f"{k:11s} {v:4d} rows  {100.0 * v / n:5.1f} percent")
    print(f"stub agreement (the {STUB_ANSWER} fraction): {counts[STUB_ANSWER]} of {n},"
          f" {100.0 * counts[STUB_ANSWER] / n:.1f} percent")


if __name__ == "__main__":
    main()
