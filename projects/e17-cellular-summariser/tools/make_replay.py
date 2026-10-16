#!/usr/bin/env python3
"""Generate the e17 replay file and its oracle. Python is allowed here as a generator.

    python projects/e17-cellular-summariser/tools/make_replay.py

Writes projects/e17-cellular-summariser/replay/day_1440.csv, a modelled day of
cellular status: 1440 rows, one a minute for 24 hours, in the shape the sampler half
of src/e17_main.c writes on the bench, header t_s,tag,v0,v1,v2,v3,v4 with tag the
operator name the modem reported ("none" when there is none), v0 the signal quality
from AT+CSQ (0 to 31, or 99 for unknown), v1 its bit error rate, v2 the registration
state from AT+CEREG? (1 home, 5 roaming, 2 searching, 3 denied, 0 neither), v3 the
access technology (7 Cat-M1 throughout, because the day is modelled with the radio
access technology locked by AT+CMNB=1, and -1 when not registered) and v4 the power
saving mode.

And tests/oracle/e17.expected.csv with the header t_s,tag,rule,model, where rule is
the decision of rules_e17.c applied in Python with the same two conditions, and model
is what the CI stub answers on the rows the loop asks it about: the grammar's first
expansion with every character class empty, {"state":"OK","report":"."}, CSV-quoted
the way common/c/src/elog.c writes it. On the other rows the model column is empty,
because project_format_prompt() returns NULL for every sample that is not a multiple
of 15 minutes, so the loop logs the rule alone. tests/oracle_check compares the two
files on those two columns, empty cells included.

The day is seeded, so running the script twice writes the same bytes. What is in it,
and why each is there:

  a mean reverting drift of signal quality in the band 4 to 24 around 11.5, so the
    OK and DEGRADED boundary at 10 is crossed in both directions many times and never
    as a step;
  two deep fades, 58 and 49 minutes, where the signal sits between 4 and 9 while the
    modem stays registered: the DEGRADED band the rule exists for;
  two registration losses, 40 and 35 minutes, through searching (2) into not
    registered (0) and back, with the signal reported as 99, unknown, while there is
    no serving cell: DOWN, and the row that a parser reading 99 as a strong signal
    would call OK;
  one denied registration minute (3) with a strong signal, the state a modem sits in
    with a subscription the network refuses;
  two operator changes: a 47 minute roaming stretch on a second operator (state 5,
    which is registered and therefore not DOWN) and a return to the first;
  isolated unknown signal minutes while registered, which are DEGRADED.

The day is deliberately eventful. A day on this bench, in a city, with the antenna at
a window, would put most of its rows in one band, and an agreement table built on it
would say nothing about the other two; so the drift band, the fades and the losses are
chosen to sample all three. That is a property of the modelled day and not a claim
about any network, the figure that shows it is drawn dashed, and the chapter says so
where it quotes the numbers.

The printed summary gives the count of each decision over all 1440 rows and over the
96 rows the model is asked about, and the stub agreement is the OK fraction of those
96. The chapter quotes both from this output.
"""
import random
from pathlib import Path

HERE = Path(__file__).resolve().parent
PROJECT = HERE.parent
REPO = PROJECT.parent.parent
REPLAY = PROJECT / "replay" / "day_1440.csv"
ORACLE = REPO / "tests" / "oracle" / "e17.expected.csv"

N = 1440
SEED = 2026
MODEL_EVERY = 15                 # samples; E17_MODEL_EVERY in src/e17_main.c
SAMPLE_SECONDS = 60.0            # E17_SAMPLE_SECONDS in src/e17_main.c
HOME = "A1"
ROAM = "Magenta"
CSQ_UNKNOWN = 99
ACT_CATM = 7
STUB_TEXT = '"{""state"":""OK"",""report"":"".""}"'   # the log's quoting of the stub

CSQ_TARGET = 11.5                # the drift's centre, just above the rule's threshold
CSQ_SIGMA = 1.2                  # one minute's step, in CSQ units
CSQ_REVERSION = 0.2              # pull back towards the centre, per minute
CSQ_FLOOR = 4.0                  # the drift's floor; the fades below use their own

# The minute spans the day is built from, each a closed interval of sample indices.
FADES = ((205, 262), (900, 948))          # registered, signal 4 to 9
LOSSES = ((410, 449), (1100, 1134))       # searching, then not registered
DENIED = (629, 629)                       # registration denied, strong signal
ROAMING = (742, 788)                      # registered and roaming on the second operator
UNKNOWN_MINUTES = (95, 96, 540, 1203)     # registered, signal reported as 99


def rule(csq, stat):
    """The same two conditions as rules_e17.c, in the same order."""
    if stat not in (1, 5):
        return "DOWN"
    if csq == CSQ_UNKNOWN or csq < 10:
        return "DEGRADED"
    return "OK"


def in_span(i, span):
    return span[0] <= i <= span[1]


def day(n=N, seed=SEED):
    rng = random.Random(seed)
    rows = []
    level = CSQ_TARGET
    for i in range(n):
        # The drift: a mean reverting random walk, clamped to a band of the CSQ scale.
        level += rng.gauss(0.0, CSQ_SIGMA) + CSQ_REVERSION * (CSQ_TARGET - level)
        level = min(24.0, max(CSQ_FLOOR, level))
        csq = int(round(level))
        ber = CSQ_UNKNOWN
        stat = 1
        oper = HOME
        psm = 1
        if any(in_span(i, s) for s in FADES):
            csq = rng.randint(4, 9)
        if in_span(i, ROAMING):
            oper = ROAM
            stat = 5
        if in_span(i, DENIED):
            stat, oper, psm, csq = 3, "none", 0, 31
        for span in LOSSES:
            if in_span(i, span):
                half = (span[0] + span[1]) // 2
                stat = 2 if i <= half else 0
                oper, psm, csq = "none", 0, CSQ_UNKNOWN
        if i in UNKNOWN_MINUTES:
            csq = CSQ_UNKNOWN
        act = ACT_CATM if stat in (1, 5) else -1
        rows.append((i * SAMPLE_SECONDS, oper, csq, ber, stat, act, psm))
    return rows


def main():
    rows = day()
    REPLAY.parent.mkdir(parents=True, exist_ok=True)
    ORACLE.parent.mkdir(parents=True, exist_ok=True)
    counts = {"OK": 0, "DEGRADED": 0, "DOWN": 0}
    asked = {"OK": 0, "DEGRADED": 0, "DOWN": 0}
    n_asked = 0
    with REPLAY.open("w", newline="\n", encoding="ascii") as f_rep, \
            ORACLE.open("w", newline="\n", encoding="ascii") as f_orc:
        f_rep.write("t_s,tag,v0,v1,v2,v3,v4\n")
        f_orc.write("t_s,tag,rule,model\n")
        for index, (t_s, oper, csq, ber, stat, act, psm) in enumerate(rows):
            r = rule(csq, stat)
            counts[r] += 1
            model = ""
            if index % MODEL_EVERY == 0:
                model = STUB_TEXT
                asked[r] += 1
                n_asked += 1
            f_rep.write("%.1f,%s,%d,%d,%d,%d,%d\n" % (t_s, oper, csq, ber, stat, act, psm))
            f_orc.write("%.6f,%s,%s,%s\n" % (t_s, oper, r, model))
    print("wrote %s and %s (seed %d)" % (REPLAY.relative_to(REPO).as_posix(),
                                         ORACLE.relative_to(REPO).as_posix(), SEED))
    for key in ("OK", "DEGRADED", "DOWN"):
        print("%-9s %4d of %d rows  %5.1f percent   asked %3d of %d"
              % (key, counts[key], N, 100.0 * counts[key] / N, asked[key], n_asked))
    print("operator changes: %d rows on %s, %d rows on %s, %d rows with none"
          % (sum(1 for row in rows if row[1] == HOME), HOME,
             sum(1 for row in rows if row[1] == ROAM), ROAM,
             sum(1 for row in rows if row[1] == "none")))
    print("rows whose decision differs from the previous row: %d"
          % sum(1 for a, b in zip(rows, rows[1:])
                if rule(a[2], a[4]) != rule(b[2], b[4])))
    print("model answers: %d of %d rows, one every %d minutes" % (n_asked, N, MODEL_EVERY))
    print("stub agreement (the OK fraction of the asked rows): %d of %d, %.1f percent"
          % (asked["OK"], n_asked, 100.0 * asked["OK"] / n_asked))


if __name__ == "__main__":
    main()
