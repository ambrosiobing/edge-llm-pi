#!/usr/bin/env python3
"""Generate the e12 replay file and its oracle. Python is allowed here as a generator.

    python projects/e12-stwin-band-reporter/tools/make_replay.py

Writes projects/e12-stwin-band-reporter/replay/band_windows.csv with the header
t_s,tag,e0,e1,e2,e3,fs_hz: one row per window, the four band energies the loop would have
computed from 4096 samples of three axes (B0 10 to 100 Hz, B1 100 to 500 Hz, B2 500 to
1500 Hz, B3 1500 to 3000 Hz, in squared milli-g units summed over bins and axes) and the
sample rate measured from the stream's timestamps. A replay of the raw 6 kHz samples would
be 100 windows by 4096 samples by 3 axes, about 1.2 million numbers and 10 MB, for a loop
whose decision depends on five numbers per window; the band rows are 100 lines and the
frame path is exercised instead by the tone test and by --frames on a captured file.

Fifty baseline windows come first: the probe at rest on its mounting, the energies drawn
with a seeded 10 percent spread around a resting spectrum. Fifty excited windows follow: a
hand-held phone vibrator or a tap on the mounting raises B1 by a factor drawn between 1.2
and 9, with B0 and B2 rising less, so most excited windows cross twice the baseline share
and a few stay under it. The excitation is modelled; nothing about the probe is claimed.

tests/oracle/e12.expected.csv carries t_s,tag,rule,model: rule is the decision of
rules_e12.c applied in Python with the same baseline (the mean B1 share of the first fifty
windows) and the same strict comparison, model is what the CI stub answers for every row,
the grammar's first expansion {"decision":"BASELINE","report":". ."}, CSV-quoted the way
common/c/src/elog.c writes it. The printed summary gives the counts; the BASELINE fraction
is the agreement the stub reaches in CI, and the chapter quotes it from this output.
"""
import random
from pathlib import Path

HERE = Path(__file__).resolve().parent
PROJECT = HERE.parent
REPO = PROJECT.parent.parent
REPLAY = PROJECT / "replay" / "band_windows.csv"
ORACLE = REPO / "tests" / "oracle" / "e12.expected.csv"
N_BASELINE = 50
N_EXCITED = 50
TAG = "stwin-bands"
WINDOW_S = 4096 / 6000.0
STUB_ANSWER = "BASELINE"
STUB_TEXT = '"{""decision"":""BASELINE"",""report"":"". .""}"'
REST = (3.0e5, 1.0e5, 6.0e4, 4.0e4)          # B0, B1, B2, B3 at rest, squared milli-g


def rule(share, baseline):
    """The same comparison as rules_e12.c: strict, and BASELINE while the baseline is 0."""
    if baseline <= 0.0:
        return "BASELINE"
    return "BAND_SHIFT" if share > 2.0 * baseline else "BASELINE"


def windows(seed=2026):
    rng = random.Random(seed)
    rows = []
    for i in range(N_BASELINE + N_EXCITED):
        e = [v * rng.lognormvariate(0.0, 0.10) for v in REST]
        if i >= N_BASELINE:
            k = rng.uniform(1.2, 9.0)
            e[1] *= k
            e[0] *= 1.0 + 0.15 * (k - 1.0)
            e[2] *= 1.0 + 0.30 * (k - 1.0)
        fs = 6000.0 + rng.gauss(0.0, 1.5)
        rows.append((i * WINDOW_S, e, fs))
    return rows


def main():
    rows = windows()
    REPLAY.parent.mkdir(parents=True, exist_ok=True)
    ORACLE.parent.mkdir(parents=True, exist_ok=True)
    counts = {"BASELINE": 0, "BAND_SHIFT": 0}
    share_sum = 0.0
    baseline = 0.0
    with REPLAY.open("w", newline="\n", encoding="ascii") as f_rep, \
            ORACLE.open("w", newline="\n", encoding="ascii") as f_orc:
        f_rep.write("t_s,tag,e0,e1,e2,e3,fs_hz\n")
        f_orc.write("t_s,tag,rule,model\n")
        for i, (t_s, e, fs) in enumerate(rows):
            e_txt = ["%.6g" % v for v in e]            # what the loop reads back
            e_val = [float(s) for s in e_txt]
            share = e_val[1] / sum(e_val)
            if i < N_BASELINE:
                share_sum += share
                b = 0.0
                if i + 1 == N_BASELINE:
                    baseline = share_sum / N_BASELINE
            else:
                b = baseline
            r = rule(share, b)
            counts[r] += 1
            f_rep.write("%.4f,%s,%s,%.1f\n" % (t_s, TAG, ",".join(e_txt), fs))
            f_orc.write("%.6f,%s,%s,%s\n" % (t_s, TAG, r, STUB_TEXT))
    n = N_BASELINE + N_EXCITED
    print("wrote %s and %s" % (REPLAY.relative_to(REPO).as_posix(),
                               ORACLE.relative_to(REPO).as_posix()))
    print("baseline B1 share over the first %d windows: %.4f" % (N_BASELINE, baseline))
    for k, v in counts.items():
        print("%-10s %4d rows  %5.1f percent" % (k, v, 100.0 * v / n))
    excited_shift = counts["BAND_SHIFT"]
    print("excited windows above twice the baseline: %d of %d" % (excited_shift, N_EXCITED))
    print("stub agreement (the %s fraction): %d of %d, %.1f percent"
          % (STUB_ANSWER, counts[STUB_ANSWER], n, 100.0 * counts[STUB_ANSWER] / n))


if __name__ == "__main__":
    main()
