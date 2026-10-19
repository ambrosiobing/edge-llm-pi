#!/usr/bin/env python3
"""Recount every figure e02 publishes, against the log column it came from.

Until Monday 19 October 2026 each of those numbers had been quoted from a tool's summary line
printed on the board and read once. This re-derives them from the rows and says, per claim,
what the chapter states and what the data gives. It also records the findings the chapter does
not yet state, so a reader can tell a confirmed figure from a new one.

    python3 tools/verify_e02_figures.py                      # print the table
    python3 tools/verify_e02_figures.py --csv <path>          # also write it as CSV
    python3 tools/verify_e02_figures.py --self-test           # prove it can fail

Exit status is 1 if any CONFIRMED claim no longer matches its column, which is the point: a
chapter whose prose drifts from its logs should fail something. Exit status is 2 if the logs
are not in this clone, loudly rather than as a skip, because a check that passes by finding
nothing has told you nothing.

--ci softens exactly one thing and nothing else, and it is a deliberate choice rather than an
oversight. Joseph's rule for this gate, Monday 19 October 2026: fail only on contradiction, not
on missing evidence. So under --ci absent logs print NOT CHECKED and return 0 while a mismatch
still returns 1. The cost is the one this repository has been bitten by before, that a skip
exits 0 and reads like a pass, so the message is made unmissable and the job step is named for
what it gates. The logs are tracked, so the only way to reach that branch is to remove one,
which is a change a reader of the diff would see.

EVERY CLAIM CARRIES ITS OWN TOLERANCE, and that is not fussiness. The first version of this
file compared everything with one relative tolerance of 0.6 percent. Asked to reject a
published 948 against a counted 949 it reported no mismatch and exited 0, because 1 is well
inside 0.6 percent of 949. A count must match exactly and a mean printed to one decimal must
not, so the kind of comparison belongs with the claim rather than in one rule for all of them.
--self-test mutates four published values and requires all four to be caught; run it after any
edit to this file. It earned its keep twice. The second bug it found was a word-valued claim
compared with a substring test: published "measured" against counted "unmeasured, 117 usable
rows" passed, because "measured" is a substring of "unmeasured". A claim whose value is a word
is compared against the whole first field now, not searched for inside a sentence.

The logs live in projects/e02-decision-loop/evidence/ and reached the authoring laptop by scp
on Sunday 18 October 2026. If they are not tracked in this repository then this script cannot
run in continuous integration, and that is a reason to track them rather than to soften this.
"""
import argparse
import csv
import io
import json
import os
import statistics
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
EV = os.path.join(HERE, "..", "projects", "e02-decision-loop", "evidence")
LITERALS = ("NORMAL", "WATCH", "ALERT")

RUN = "e02_run.csv"
HUNDRED12 = "e02_npredict12.csv"
CUT12 = "e02_run_npredict12.csv"
RES_SAT = "e02_variant_a_restore_2026-10-17.csv"
COLD_DISC = "e02_variant_a_cold_2026-10-18_discarded.csv"
RES_PAIR = "e02_variant_a_restore_pair.csv"
COLD_PAIR = "e02_variant_a_cold_pair.csv"
RUN12 = "e02_run_1000_npredict12.csv"      # the clause-one run of Monday 19 October 2026
TRACE12 = "e02_throttle_1000.csv"          # its register, clock and temperature trace
RES_OK = "e02_variant_a_restore_2026-10-19.csv"   # the accepted pair, restore half
COLD_OK = "e02_variant_a_cold_2026-10-19.csv"     # the accepted pair, cold half
TRACE_OK = "e02_throttle_pair_2026-10-19.csv"     # the accepted pair's trace
NEEDED = (RUN, HUNDRED12, CUT12, RES_SAT, COLD_DISC, RES_PAIR, COLD_PAIR, RUN12, TRACE12,
          RES_OK, COLD_OK, TRACE_OK)

EXACT = "exact"        # the two strings must be identical
# HEAD has no claim using it since Monday 19 October 2026, when the one word-valued claim
# it served was renamed. It is kept because the bug it was written for is worth not
# reintroducing: a substring test passed a published "measured" against a counted
# "unmeasured, 117 usable rows". Any future claim whose value is a word uses this, never
# CONTAINS.
HEAD = "head"          # the published text must equal the counted text up to the first comma
CONTAINS = "contains"  # the published text must appear in the counted text
UNCHECKED = "n/a"      # a NEW or DEFECT row: there is no published value to compare against
PAIR = RES_PAIR + " and " + COLD_PAIR


def read_bytes(name):
    with io.open(os.path.join(EV, name), "rb") as fh:
        return fh.read()


def rows(name):
    """Rows of a log, stopping at the first NUL byte.

    A log from a run the board interrupted has its size recorded and its tail never written,
    so it reads back as NUL. Everything before the first NUL is real and everything after it
    is absent, and the two must not be mixed. See the zeroed-tail row in the table below.
    """
    b = read_bytes(name)
    cut = b.find(b"\x00")
    text = (b if cut < 0 else b[:cut]).decode("utf-8")
    return list(csv.DictReader(io.StringIO(text)))


def closes(text):
    """The literal of a complete JSON object, or None when the object never closed."""
    if not text:
        return None
    try:
        obj = json.loads(text)
    except Exception:
        return None
    if isinstance(obj, dict) and obj.get("decision") in LITERALS:
        return obj["decision"]
    return None


def literal_in(text):
    """The one legal literal present in the text, even when the object is truncated."""
    if not text:
        return None
    hit = [lit for lit in LITERALS if lit in text]
    return hit[0] if len(hit) == 1 else None


def mean_of(data, field):
    return statistics.mean(float(x[field]) for x in data)


def matches(published, counted, tol):
    """True when the counted value still supports the published one, by this claim's rule."""
    if tol == EXACT:
        return published == counted
    if tol == HEAD:
        return published == counted.split(",")[0].strip()
    if tol == CONTAINS:
        return published in counted
    if tol == UNCHECKED:
        return True
    try:
        return abs(float(published) - float(counted)) <= float(tol)
    except (TypeError, ValueError):
        return False


def build(mutate=None):
    """The table. mutate maps a claim to a wrong published value, for --self-test."""
    run = rows(RUN)
    h12 = rows(HUNDRED12)
    cut = rows(CUT12)
    rp, cp = rows(RES_PAIR), rows(COLD_PAIR)
    rs, cd = rows(RES_SAT), rows(COLD_DISC)
    r12, tr = rows(RUN12), rows(TRACE12)
    rok, cok, trok = rows(RES_OK), rows(COLD_OK), rows(TRACE_OK)

    # Clause two, recounted. The low-clock question needs the span and not the minimum: a
    # minimum over a whole trace cannot tell the idle governor from a reduction, and the
    # samples that matter are the ones inside the two passes.
    prok = [float(x["prompt_ms"]) for x in rok]
    pcok = [float(x["prompt_ms"]) for x in cok]
    ratio_ok = statistics.mean(pcok) / statistics.mean(prok)
    perok = [c / r for c, r in zip(pcok, prok)]
    dirty_ok = sum(1 for x in trok if x["throttled"] != "0x0")
    t0 = int(trok[0]["unix_s"])
    in_pass = [x for x in trok if 12 <= int(x["unix_s"]) - t0 <= 1047]
    low_in_pass = sum(1 for x in in_pass if int(x["arm_hz"]) < 1700000000)
    peak_ok = max(float(x["temp_c"]) for x in trok)
    gok = statistics.mean(float(x["generate_ms"]) for x in rok)
    gcok = statistics.mean(float(x["generate_ms"]) for x in cok)

    # Clause one's own numbers, recounted rather than carried.
    closed12 = [closes(x["model"]) for x in r12]
    malformed12 = sum(1 for c in closed12 if c is None)
    agree12 = sum(1 for x, c in zip(r12, closed12) if c == x["rule"])
    split12 = {}
    for c in closed12:
        split12[c] = split12.get(c, 0) + 1
    rule12 = {}
    for x in r12:
        rule12[x["rule"]] = rule12.get(x["rule"], 0) + 1
    watch_hit = sum(1 for x, c in zip(r12, closed12) if x["rule"] == "WATCH" and c == "WATCH")
    alert_hit = sum(1 for x, c in zip(r12, closed12) if x["rule"] == "ALERT" and c == "ALERT")
    # The literal each run chose, to show the budget changed closure and not choice.
    lit8 = [literal_in(x["model"]) for x in run]
    lit12 = [literal_in(x["model"]) for x in r12]
    same_lit = sum(1 for a, b in zip(lit8, lit12) if a == b)
    dirty = sum(1 for x in tr if x["throttled"] != "0x0")

    rule = {}
    for x in run:
        rule[x["rule"]] = rule.get(x["rule"], 0) + 1
    pr = [float(x["prompt_ms"]) for x in rp]
    pc = [float(x["prompt_ms"]) for x in cp]
    per = [c / r for c, r in zip(pc, pr)]
    cutb = read_bytes(CUT12)
    zeroed = (len(cutb) - cutb.find(b"\x00")) if b"\x00" in cutb else 0

    # (status, claim, published, counted, source, tolerance)
    T = [
        ("CONFIRMED", "rows in the thousand-decision run",
         "1000", "%d" % len(run), RUN, EXACT),
        ("CONFIRMED", "objects that never closed", "949",
         "%d" % sum(1 for x in run if closes(x["model"]) is None), RUN, EXACT),
        ("CONFIRMED", "rule column NORMAL", "823", "%d" % rule.get("NORMAL", 0), RUN, EXACT),
        ("CONFIRMED", "rule column WATCH", "110", "%d" % rule.get("WATCH", 0), RUN, EXACT),
        ("CONFIRMED", "rule column ALERT", "67", "%d" % rule.get("ALERT", 0), RUN, EXACT),
        ("CONFIRMED", "agreement on the quiet hundred, budget 12", "4 of 100",
         "%d of %d" % (sum(1 for x in h12 if closes(x["model"]) == x["rule"]), len(h12)),
         HUNDRED12, EXACT),
        ("CONFIRMED", "restore mean prompt_ms, Saturday 17 October 2026", "1320.6",
         "%.1f" % mean_of(rs, "prompt_ms"), RES_SAT, 0.05),
        ("CONFIRMED", "discarded cold mean prompt_ms", "5830.4",
         "%.1f" % mean_of(cd, "prompt_ms"), COLD_DISC, 0.05),
        ("CONFIRMED", "pair restore mean prompt_ms", "1325.6",
         "%.1f" % statistics.mean(pr), RES_PAIR, 0.05),
        ("CONFIRMED", "pair cold mean prompt_ms", "5927.5",
         "%.1f" % statistics.mean(pc), COLD_PAIR, 0.05),
        ("CONFIRMED", "pair restore mean generate_ms", "1524.8",
         "%.1f" % mean_of(rp, "generate_ms"), RES_PAIR, 0.05),
        ("CONFIRMED", "pair cold mean generate_ms", "1560.6",
         "%.1f" % mean_of(cp, "generate_ms"), COLD_PAIR, 0.05),
        ("CONFIRMED", "the ratio of the two means", "4.47",
         "%.3f" % (statistics.mean(pc) / statistics.mean(pr)), PAIR, 0.005),
        # Until Monday 19 October 2026 this read "unmeasured" and the interrupted log was
        # its only evidence. It is measured now, so the claim changed rather than the file.
        ("CONFIRMED", "the interrupted budget-12 run is still 117 usable rows", "117",
         "%d" % len(cut), CUT12, EXACT),

        ("CONFIRMED", "clause two's verdict", "PASSES",
         "PASSES" if (ratio_ok >= 2.0 and dirty_ok == 0 and len(rok) == 100
                      and len(cok) == 100) else "FAILS",
         RES_OK + " and " + COLD_OK, EXACT),
        ("CONFIRMED", "the accepted ratio", "4.45",
         "%.4f" % ratio_ok, RES_OK + " and " + COLD_OK, 0.005),
        ("CONFIRMED", "accepted restore mean prompt_ms", "1318.6",
         "%.1f" % statistics.mean(prok), RES_OK, 0.05),
        ("CONFIRMED", "accepted cold mean prompt_ms", "5867.3",
         "%.1f" % statistics.mean(pcok), COLD_OK, 0.05),
        ("CONFIRMED", "accepted rows below the acceptance of 2.0", "0 of 100",
         "%d of %d" % (sum(1 for v in perok if v < 2.0), len(perok)),
         RES_OK + " and " + COLD_OK, EXACT),
        ("CONFIRMED", "dirty-register samples in the accepted pair", "0 of 1328",
         "%d of %d" % (dirty_ok, len(trok)), TRACE_OK, EXACT),
        ("CONFIRMED", "low-clock samples inside either accepted pass", "0",
         "%d" % low_in_pass, TRACE_OK, EXACT),
        ("CONFIRMED", "the accepted pair's peak temperature", "72.5",
         "%.1f" % peak_ok, TRACE_OK, 0.05),
        ("CONFIRMED", "generation across the accepted pair", "0.91 percent",
         "%.2f percent" % abs(100 * (gcok / gok - 1)), RES_OK + " and " + COLD_OK, EXACT),

        ("CONFIRMED", "clause one's verdict", "PASSES",
         "PASSES" if (malformed12 == 0 and len(r12) == 1000
                      and not any(not x["model"] for x in r12)) else "FAILS",
         RUN12, EXACT),
        ("CONFIRMED", "clause one, rows in the repaired thousand", "1000",
         "%d" % len(r12), RUN12, EXACT),
        ("CONFIRMED", "clause one, objects that never closed", "0",
         "%d" % malformed12, RUN12, EXACT),
        ("CONFIRMED", "clause one, tokens the budget of 12 actually needed", "8,9",
         ",".join(str(v) for v in sorted({int(x["n_generated"]) for x in r12})),
         RUN12, EXACT),
        ("CONFIRMED", "agreement over the repaired thousand", "87 of 1000",
         "%d of %d" % (agree12, len(r12)), RUN12, EXACT),
        ("CONFIRMED", "the model's literal split, repaired", "949 ALERT, 51 NORMAL",
         "%d ALERT, %d NORMAL" % (split12.get("ALERT", 0), split12.get("NORMAL", 0)),
         RUN12, EXACT),
        ("CONFIRMED", "rows choosing the same literal at both budgets", "1000 of 1000",
         "%d of %d" % (same_lit, len(r12)), RUN + " and " + RUN12, EXACT),
        ("CONFIRMED", "WATCH recall over the thousand", "0 of 110",
         "%d of %d" % (watch_hit, rule12.get("WATCH", 0)), RUN12, EXACT),
        ("CONFIRMED", "ALERT recall over the thousand", "66 of 67",
         "%d of %d" % (alert_hit, rule12.get("ALERT", 0)), RUN12, EXACT),
        ("CONFIRMED", "what a constant NORMAL would score", "823 of 1000",
         "%d of %d" % (rule12.get("NORMAL", 0), len(r12)), RUN12, EXACT),
        ("CONFIRMED", "repaired mean prompt_ms", "2993.3",
         "%.1f" % mean_of(r12, "prompt_ms"), RUN12, 0.05),
        ("CONFIRMED", "repaired mean generate_ms", "2008.1",
         "%.1f" % mean_of(r12, "generate_ms"), RUN12, 0.05),
        ("CONFIRMED", "dirty-register samples in the clause-one trace", "0 of 6000",
         "%d of %d" % (dirty, len(tr)), TRACE12, EXACT),

        ("NEW", "generation budget was reached on every row", "not stated",
         "n_generated is 8 on %d of %d" % (
             sum(1 for x in run if x["n_generated"] == "8"), len(run)), RUN, UNCHECKED),
        ("NEW", "tokens an object needs at budget 12", "not stated",
         "ALERT 9, NORMAL 8, so the failure was one token", HUNDRED12, UNCHECKED),
        ("NEW", "agreement of the malformed run has two values", "not stated",
         "%d of 1000 closed-only, %d of 1000 by literal in the text" % (
             sum(1 for x in run if closes(x["model"]) == x["rule"]),
             sum(1 for x in run if literal_in(x["model"]) == x["rule"])), RUN, UNCHECKED),
        ("NEW", "rows of the pair below the acceptance of 2.0", "not stated",
         "%d of %d, per-row ratio %.3f to %.3f" % (
             sum(1 for v in per if v < 2.0), len(per), min(per), max(per)), PAIR, UNCHECKED),
        ("NEW", "thermal drift within the cold pass", "not stated",
         "%.1f ms first quarter to %.1f ms last, %.1f percent" % (
             statistics.mean(pc[:25]), statistics.mean(pc[75:]),
             100.0 * (statistics.mean(pc[75:]) / statistics.mean(pc[:25]) - 1)),
         COLD_PAIR, CONTAINS),
        ("NEW", "first row is not an outlier in either pass", "not stated",
         "restore %+.1f percent, cold %+.1f percent against rows 1 on" % (
             100.0 * (pr[0] / statistics.mean(pr[1:]) - 1),
             100.0 * (pc[0] / statistics.mean(pc[1:]) - 1)), PAIR, UNCHECKED),

        ("DEFECT", "bytes of the interrupted log that are zeroed", "not stated",
         "%d of %d, about %d rows counted in the size and lost" % (
             zeroed, len(cutb), zeroed // 111), CUT12, UNCHECKED),
    ]
    if mutate:
        T = [(s, c, mutate.get(c, p), g, src, t) for s, c, p, g, src, t in T]
    return T


def run_table(T, quiet=False):
    width = max(len(r[1]) for r in T)
    bad = []
    for status, claim, pub, got, src, tol in T:
        ok = True if status != "CONFIRMED" else matches(pub, got, tol)
        if not ok:
            bad.append(claim)
        if not quiet:
            print("%-9s %-*s  published %-12s  counted %s%s"
                  % (status, width, claim, pub, got,
                     "   <-- DOES NOT MATCH" if not ok else ""))
    if not quiet:
        print()
        print("%d claims checked, %d confirmed figures do not match their column"
              % (len(T), len(bad)))
    return bad


def self_test():
    """Mutate four published values and require every one to be caught."""
    # One case per comparison kind the table actually uses: an exact integer, an exact
    # string, a numeric tolerance and a word. The fourth case named
    # "repaired agreement over the full thousand" until Monday 19 October 2026, when that
    # claim was renamed because the figure stopped being unmeasured. The self-test then
    # failed, correctly: it was mutating a claim no longer in the table, so nothing could be
    # caught. A mutation list is itself a thing that goes stale, and the only reason that was
    # noticed is that this test requires every case to be caught rather than reporting a rate.
    cases = {
        "objects that never closed": "948",
        "agreement over the repaired thousand": "88 of 1000",
        "pair cold mean prompt_ms": "5927.6",
        "clause one's verdict": "FAILS",
    }
    print("self-test: four mutations, each must be caught")
    missed = []
    for claim, wrong in cases.items():
        bad = run_table(build({claim: wrong}), quiet=True)
        caught = claim in bad
        print("  %-45s published %-10s %s"
              % (claim, wrong, "caught" if caught else "MISSED"))
        if not caught:
            missed.append(claim)
    clean = run_table(build(), quiet=True)
    print("  %-45s %s" % ("and the unmutated table",
                          "clean" if not clean else "DIRTY: %s" % clean))
    if missed or clean:
        print("self-test FAILED: %d missed, %d false positives" % (len(missed), len(clean)))
        return 1
    print("self-test passed: it catches a count off by one, a mean off by 0.1, a ratio off")
    print("by 0.01 and a changed word, and reports nothing on the real table.")
    return 0


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--csv", help="also write the table to this path")
    ap.add_argument("--self-test", action="store_true",
                    help="mutate published values and require each to be caught")
    ap.add_argument("--ci", action="store_true",
                    help="fail only on a mismatch; absent logs print NOT CHECKED and pass")
    args = ap.parse_args()

    missing = [n for n in NEEDED if not os.path.exists(os.path.join(EV, n))]
    if missing:
        bar = "!" * 78
        for line in (bar,
                     "verify_e02_figures: NOT CHECKED. These logs are not in this clone:",
                     "    " + ", ".join(missing),
                     "Nothing was compared, which is not the same as nothing being wrong.",
                     "The figures in e02's chapter are unverified in this run.",
                     bar):
            print(line, file=sys.stderr)
        return 0 if args.ci else 2

    if args.self_test:
        return self_test()

    T = build()
    bad = run_table(T)
    if args.csv:
        with io.open(args.csv, "w", encoding="utf-8", newline="") as fh:
            w = csv.writer(fh, lineterminator="\n")
            w.writerow(["status", "claim", "published", "counted", "source"])
            for status, claim, pub, got, src, tol in T:
                w.writerow([status, claim, pub, got, src])
        print("written: %s" % args.csv)
    return 1 if bad else 0


if __name__ == "__main__":
    sys.exit(main())
