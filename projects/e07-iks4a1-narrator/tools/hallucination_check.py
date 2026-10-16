#!/usr/bin/env python3
"""Count the numbers in every e07 summary that are not among that event's inputs, and
print the trend agreement table beside the count.

    python projects/e07-iks4a1-narrator/tools/hallucination_check.py <log.csv> [--md]
           [--prompt prompt.txt] [--verbose] [--fail-over N]

Reads the CSV common/c/src/elog.c writes. A row whose model column is empty was logged
with the rule alone (59 rows of 60 in this project) and is not scored. For every other
row the model column is parsed as JSON with the keys "trend" and "summary" of
grammars/e07_pressure.gbnf.

The inputs of an event are the values of the raw column (the nine event values, joined
by ';'), the change v0 - v1 that the user text states, and every number in the system
prompt (30 minutes, 1 hPa). A number in the summary with d decimals is present when
some input rounds to it at d decimals, so "22 C" matches 22.11 and "1013.2" matches
1013.23; anything else is counted as a number the model made up. The count is the
acceptance figure the chapter publishes; --fail-over N turns it into an exit status.

The trend key is scored exactly as tools/agreement.py scores the house key
"decision": N, the agreement rate, the confusion table (rule rows, model columns) and
every disagreement with its log line. The summary length is checked against the
grammar's 44 to 398 characters; the stub's ". . ." is reported as such, not as a fault.
"""
import argparse
import csv
import json
import re
import sys
from pathlib import Path

HERE = Path(__file__).resolve().parent
NUMBER = re.compile(r"(?<![A-Za-z0-9.])[+-]?\d+(?:\.\d+)?(?![A-Za-z0-9.]|\d)")
TRENDS = ("STABLE", "RISING", "FALLING")
MIN_LEN, MAX_LEN = 44, 398


def numbers_in(text):
    out = []
    for m in NUMBER.finditer(text):
        s = m.group(0)
        d = len(s.split(".")[1]) if "." in s else 0
        out.append((s, float(s), d))
    return out


def present(value, decimals, inputs):
    """inputs holds (x, max_decimals): an event value matches at any precision the
    summary uses, a prompt constant only at the precision the prompt wrote it, so the
    prompt's "1 hPa" does not absorb a "1.00 g" the model made up."""
    for x, max_decimals in inputs:
        if decimals <= max_decimals and round(x, decimals) == round(value, decimals):
            return True
    return False


def parse_model(text):
    text = text.strip()
    if not text:
        return None, None
    try:
        obj = json.loads(text)
    except ValueError:
        return "", text
    if not isinstance(obj, dict):
        return "", text
    return str(obj.get("trend", "")), str(obj.get("summary", ""))


def main():
    ap = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    ap.add_argument("log")
    ap.add_argument("--md", action="store_true", help="print Markdown tables")
    ap.add_argument("--prompt", default=str(HERE.parent / "prompt.txt"))
    ap.add_argument("--verbose", action="store_true", help="list every counted number")
    ap.add_argument("--fail-over", type=int, default=-1,
                    help="exit 1 when the count exceeds N; the default never fails")
    args = ap.parse_args()

    prompt_numbers = [(v, d) for _, v, d in numbers_in(Path(args.prompt).read_text())]
    with open(args.log, newline="", encoding="utf-8") as fh:
        reader = csv.DictReader(fh)
        need = ("rule", "model", "raw")
        if reader.fieldnames is None or any(c not in reader.fieldnames for c in need):
            raise SystemExit("hallucination_check: %s lacks rule, model and raw" % args.log)
        rows = list(reader)

    n_total = len(rows)
    n_scored = n_agree = n_made_up = n_no_trend = n_short = n_long = n_stub = 0
    confusion = {}
    disagreements = []
    made_up = []
    for number, row in enumerate(rows, 2):
        trend, summary = parse_model(row["model"])
        if trend is None:
            continue
        n_scored += 1
        rule = row["rule"].strip()
        values = [float(x) for x in row["raw"].split(";") if x]
        if len(values) >= 2:
            values.append(values[0] - values[1])
        inputs = [(x, 6) for x in values] + prompt_numbers
        if trend not in TRENDS:
            n_no_trend += 1
        else:
            confusion.setdefault(rule, {}).setdefault(trend, 0)
            confusion[rule][trend] += 1
            if trend == rule:
                n_agree += 1
            else:
                disagreements.append((number, row.get("t_s", ""), rule, trend))
        if row.get("engine", "").strip() == "stub":
            n_stub += 1
        elif len(summary) < MIN_LEN:
            n_short += 1
        elif len(summary) > MAX_LEN:
            n_long += 1
        for text, value, d in numbers_in(summary):
            if not present(value, d, inputs):
                n_made_up += 1
                made_up.append((number, text, summary))

    rate = (100.0 * n_agree / n_scored) if n_scored else 0.0
    rules = sorted(confusion)
    if args.md:
        print("| N | asked | agreement | numbers not in the inputs | no trend key |")
        print("|---|---|---|---|---|")
        print("| %d | %d | %.1f percent | %d | %d |" % (n_total, n_scored, rate, n_made_up,
                                                        n_no_trend))
        print()
        print("| rule \\ model | " + " | ".join(TRENDS) + " |")
        print("|---|" + "---|" * len(TRENDS))
        for r in rules:
            print("| %s | " % r + " | ".join(str(confusion[r].get(m, 0)) for m in TRENDS) + " |")
    else:
        print("N %d, asked %d, agreement %d (%.1f percent), numbers not in the inputs %d, "
              "answers without a trend key %d" % (n_total, n_scored, n_agree, rate, n_made_up,
                                                   n_no_trend))
        print("summaries under %d characters %d, over %d characters %d, stub answers %d"
              % (MIN_LEN, n_short, MAX_LEN, n_long, n_stub))
        width = max([len(r) for r in rules] + [12])
        print("rule \\ model".ljust(width) + "".join(m.rjust(12) for m in TRENDS))
        for r in rules:
            print(r.ljust(width) + "".join(str(confusion[r].get(m, 0)).rjust(12)
                                           for m in TRENDS))
    if disagreements:
        print()
        print("disagreements (log line, t_s, rule, model):")
        for item in disagreements:
            print("  %d, %s, %s, %s" % item)
    if made_up and (args.verbose or len(made_up) <= 20):
        print()
        print("numbers not in the inputs (log line, number, summary):")
        for number, text, summary in made_up:
            print("  %d, %s, %s" % (number, text, summary[:80]))
    if 0 <= args.fail_over < n_made_up:
        return 1
    return 0


if __name__ == "__main__":
    sys.exit(main())
