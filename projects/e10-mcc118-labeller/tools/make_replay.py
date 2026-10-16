#!/usr/bin/env python3
"""Generate the e10 replay file and its oracle, and check a bench log's step amplitude.
Python is allowed here as a generator and as an instrument harness.

    python projects/e10-mcc118-labeller/tools/make_replay.py
    python projects/e10-mcc118-labeller/tools/make_replay.py --check <log.csv> <volts>

The first form writes projects/e10-mcc118-labeller/replay/stimuli_300.csv with the
header t_s,tag,v0,...,v15, one row per 2 ms window of 200 samples at 100 kS/s, and
tests/oracle/e10.expected.csv with the header t_s,tag,rule,model. Each window is
synthesised as 200 samples (a step, a ramp or a flat level, plus noise, quantised to
the HAT's 4.88 mV step over its -10 to +10 V range) and then summarised with the SAME
arithmetic as src/io_mcc118.c: v0 the minimum, v1 the maximum, v2 the steepest change
between two neighbouring samples, v3 to v15 the thirteen bin means with the bounds
k * n // 13 and (k + 1) * n // 13. The tag carries the kind and the amplitude
commanded at the DAC, for example step:2.0, ramp:1.5, noise. A row is at most 98
columns (tools/line_check.py holds every file to 99): t_s with three decimals, the
three summaries with three, the thirteen points with two, which is also how the user
text prints them.

The rule column is the decision of rules_e10.c applied in Python with the same two
thresholds on the values as written to the file (rounded as above, as the C loop will
read them), and the model column is what the CI stub answers for every row:
the grammar's first expansion, {"decision":"NOISE"}, CSV-quoted the way
common/c/src/elog.c writes it, so that tests/oracle_check compares the two files byte
for byte on those two columns.

The second form is the bench's amplitude check: it reads a log written by elog.c, takes
every row whose rule column is STEP, estimates each window's amplitude as the
difference between the last and the first averaged point (the two settled ends of the
window, which is why the generator keeps the step edge inside samples 40 to 160), and
compares the median over those rows with the commanded volts. Exit 1 when the two
differ by more than 2 percent, which is the acceptance condition of the chapter. The
same check runs over the generated file at the end of the first form, per tag, as a
self-test of the check.

The generator is seeded, so running it twice writes the same bytes.
"""
import csv
import random
import statistics
import sys
from pathlib import Path

HERE = Path(__file__).resolve().parent
PROJECT = HERE.parent
REPO = PROJECT.parent.parent
REPLAY = PROJECT / "replay" / "stimuli_300.csv"
ORACLE = REPO / "tests" / "oracle" / "e10.expected.csv"

N_EACH = 100                        # steps, ramps and noise windows each
WINDOW = 200                        # samples: 2 ms at 100 kS/s
POINTS = 13                         # IO_MAX_VALUES 16 minus the three summaries
LSB = 20.0 / 4096.0                 # the HAT's 12 bits over -10 to +10 V
AMPLITUDES = [0.5, 1.0, 1.5, 2.0, 2.5, 3.0]   # commanded at the DAC, all inside 0 to 3.3 V
LOW_MIN, HIGH_MAX = 0.20, 3.20      # the DAC's settled levels stay off both rails
STUB_ANSWER = "NOISE"
STUB_TEXT = '"{""decision"":""NOISE""}"'      # the log's quoting of {"decision":"NOISE"}
TOLERANCE = 0.02


def rule(v):
    """The same thresholds as rules_e10.c, in the same order, on the same values."""
    if len(v) < 3:
        return "NOISE"
    if v[1] - v[0] <= 0.20:
        return "NOISE"
    if v[2] > 0.4 * (v[1] - v[0]):
        return "STEP"
    return "RAMP"


def summarise(s):
    """The window summary of src/io_mcc118.c, e10_summarise, line for line."""
    n = len(s)
    lo = hi = s[0]
    jump = 0.0
    for i in range(1, n):
        d = s[i] - s[i - 1]
        if s[i] < lo:
            lo = s[i]
        if s[i] > hi:
            hi = s[i]
        if d < 0.0:
            d = -d
        if d > jump:
            jump = d
    out = [lo, hi, jump]
    for k in range(POINTS):
        a, b = k * n // POINTS, (k + 1) * n // POINTS
        total = 0.0
        for i in range(a, b):
            total += s[i]
        out.append(total / (b - a) if b > a else s[n - 1])
    return out


def quantise(x):
    return round(x / LSB) * LSB


def window_step(rng, amp):
    low = rng.uniform(LOW_MIN, HIGH_MAX - amp)
    rising = rng.random() < 0.5
    edge = rng.randint(40, 160)                  # both ends of the window stay settled
    sigma = 0.004
    s = []
    for i in range(WINDOW):
        level = low + amp if (i >= edge) == rising else low
        s.append(quantise(level + rng.gauss(0.0, sigma)))
    return s


def window_ramp(rng, amp):
    low = rng.uniform(LOW_MIN, HIGH_MAX - amp)
    rising = rng.random() < 0.5
    length = rng.randint(40, 150)                # 0.4 to 1.5 ms of slope
    start = rng.randint(10, WINDOW - length - 10)
    sigma = 0.004
    s = []
    for i in range(WINDOW):
        frac = min(1.0, max(0.0, (i - start) / length))
        level = low + amp * (frac if rising else 1.0 - frac)
        s.append(quantise(level + rng.gauss(0.0, sigma)))
    return s


def window_noise(rng):
    level = rng.uniform(LOW_MIN, HIGH_MAX)
    sigma = 0.004 if rng.random() < 0.8 else 0.02      # a fifth are noisier, still in band
    return [quantise(level + rng.gauss(0.0, sigma)) for _ in range(WINDOW)]


def generate(seed=2026):
    rng = random.Random(seed)
    kinds = ["step"] * N_EACH + ["ramp"] * N_EACH + ["noise"] * N_EACH
    rng.shuffle(kinds)
    rows = []
    for i, kind in enumerate(kinds):
        if kind == "step":
            amp = AMPLITUDES[i % len(AMPLITUDES)]
            s = window_step(rng, amp)
        elif kind == "ramp":
            amp = AMPLITUDES[i % len(AMPLITUDES)]
            s = window_ramp(rng, amp)
        else:
            amp = 0.0
            s = window_noise(rng)
        raw = summarise(s)
        values = [float("%.3f" % x) for x in raw[:3]] + [float("%.2f" % x) for x in raw[3:]]
        tag = "noise" if kind == "noise" else "%s:%.1f" % (kind, amp)
        rows.append((0.002 * i, tag, values))
    return rows


def row_text(t_s, tag, values):
    """One replay row as written: the same rounding generate() applied to the values."""
    return "%.3f,%s,%s,%s" % (t_s, tag, ",".join("%.3f" % x for x in values[:3]),
                              ",".join("%.2f" % x for x in values[3:]))


def amplitude_of(values):
    """The settled ends of the window: the last averaged point minus the first."""
    return abs(values[15] - values[3])


def check_rows(rows, commanded, label):
    """rows: (rule, values) pairs. Prints the verdict and returns True when inside 2 percent."""
    amps = [amplitude_of(v) for r, v in rows if r == "STEP"]
    if not amps:
        print("%s: no STEP rows, nothing to check" % label)
        return False
    median = statistics.median(amps)
    error = abs(median - commanded) / commanded
    verdict = "pass" if error <= TOLERANCE else "FAIL"
    print("%s: %d STEP rows, median amplitude %.4f V against %.3f V commanded, %.2f percent, %s"
          % (label, len(amps), median, commanded, 100.0 * error, verdict))
    return error <= TOLERANCE


def check_log(path, commanded):
    """A log from common/c/src/elog.c: the raw column joins the values with ';'."""
    rows = []
    with open(path, newline="", encoding="utf-8") as fh:
        for row in csv.DictReader(fh):
            values = [float(x) for x in row["raw"].split(";") if x]
            if len(values) >= 16:
                rows.append((row["rule"].strip(), values))
    ok = check_rows(rows, commanded, Path(path).name)
    return 0 if ok else 1


def main(argv):
    if len(argv) == 3 and argv[0] == "--check":
        return check_log(argv[1], float(argv[2]))
    rows = generate()
    REPLAY.parent.mkdir(parents=True, exist_ok=True)
    ORACLE.parent.mkdir(parents=True, exist_ok=True)
    counts = {"NOISE": 0, "RAMP": 0, "STEP": 0}
    header = "t_s,tag," + ",".join("v%d" % i for i in range(16))
    with REPLAY.open("w", newline="\n", encoding="ascii") as f_rep, \
            ORACLE.open("w", newline="\n", encoding="ascii") as f_orc:
        f_rep.write(header + "\n")
        f_orc.write("t_s,tag,rule,model\n")
        for t_s, tag, values in rows:
            r = rule(values)
            counts[r] += 1
            f_rep.write(row_text(t_s, tag, values) + "\n")
            f_orc.write("%.3f,%s,%s,%s\n" % (t_s, tag, r, STUB_TEXT))
    print("wrote %s and %s" % (REPLAY.relative_to(REPO).as_posix(),
                               ORACLE.relative_to(REPO).as_posix()))
    n = len(rows)
    for k, v in counts.items():
        print("%-6s %4d rows  %5.1f percent" % (k, v, 100.0 * v / n))
    print("stub agreement (the %s fraction): %d of %d, %.1f percent"
          % (STUB_ANSWER, counts[STUB_ANSWER], n, 100.0 * counts[STUB_ANSWER] / n))
    # Self-test of the amplitude check over the synthetic steps, per commanded amplitude.
    ok = True
    for amp in AMPLITUDES:
        tagged = [(rule(v), v) for t, tag, v in rows if tag == "step:%.1f" % amp]
        ok = check_rows(tagged, amp, "synthetic step:%.1f" % amp) and ok
    return 0 if ok else 1


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
