#!/usr/bin/env python3
"""The two acceptance fractions of project e03, read from one event log.

    python projects/e03-led-explainer/tools/reason_check.py <log.csv>
    python projects/e03-led-explainer/tools/reason_check.py <log.csv> --md

Reads the CSV common/c/src/elog.c writes (columns t_s, tag, raw, rule, model, ...). The
model column is the JSON object of grammars/e03_colour.gbnf, {"colour": ..., "reason":
...}; tools/agreement.py reads a "decision" key and so does not apply here, which is why
this checker exists. For every row it recomputes which field the rule triggered on from
the raw column (v0;v1, the level and the slope, with the thresholds of rules_e03.c:
level above 80 or slope above 10 is RED, level above 60 is YELLOW) and prints

  colour agreement   rows whose colour equals the rule column, over the rows scored
  reason names field rows whose reason contains the word for the triggering field,
                     "level" or "slope" (either when both trigger), case-insensitively

plus the 3 by 3 confusion table, the count of rows with no parseable object (a failure
of the chapter, never of the model) and every disagreement with its log line. Rows whose
model column is empty are counted separately and never as agreement. A stub log scores
the GREEN fraction on the first line and zero on the second, by construction.
"""
import argparse
import csv
import json
import sys

COLOURS = ("GREEN", "YELLOW", "RED")


def cause_words(raw):
    parts = raw.split(";")
    v0 = float(parts[0]) if len(parts) > 0 and parts[0] else 0.0
    v1 = float(parts[1]) if len(parts) > 1 and parts[1] else 0.0
    if v0 > 80.0 and v1 > 10.0:
        return ("level", "slope")
    if v1 > 10.0:
        return ("slope",)
    return ("level",)


def parse_model(text):
    text = text.strip()
    if not text:
        return None, None, "empty"
    try:
        value = json.loads(text)
    except ValueError:
        return None, None, "unparsed"
    if not isinstance(value, dict) or "colour" not in value or "reason" not in value:
        return None, None, "unparsed"
    return str(value["colour"]), str(value["reason"]), "ok"


def load(path):
    with open(path, newline="", encoding="utf-8") as fh:
        reader = csv.DictReader(fh)
        need = ("rule", "model", "raw")
        if reader.fieldnames is None or any(c not in reader.fieldnames for c in need):
            raise SystemExit("reason_check: %s lacks raw, rule and model columns" % path)
        return list(reader)


def main():
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("log")
    parser.add_argument("--md", action="store_true", help="print Markdown tables")
    args = parser.parse_args()

    rows = load(args.log)
    n_total = len(rows)
    n_empty = n_unparsed = 0
    colour_ok = reason_ok = 0
    confusion = {r: {m: 0 for m in COLOURS} for r in COLOURS}
    disagreements = []
    for number, row in enumerate(rows, 2):
        rule = row["rule"].strip()
        colour, reason, state = parse_model(row["model"])
        if state == "empty":
            n_empty += 1
            continue
        if state == "unparsed":
            n_unparsed += 1
            continue
        if rule in confusion and colour in confusion[rule]:
            confusion[rule][colour] += 1
        words = cause_words(row["raw"])
        low = reason.lower()
        named = any(w in low for w in words)
        if colour == rule:
            colour_ok += 1
        if named:
            reason_ok += 1
        if colour != rule or not named:
            disagreements.append((number, row.get("t_s", ""), rule, colour,
                                  " or ".join(words), reason))

    n_scored = n_total - n_empty - n_unparsed
    c_rate = 100.0 * colour_ok / n_scored if n_scored else 0.0
    r_rate = 100.0 * reason_ok / n_scored if n_scored else 0.0

    if args.md:
        print("| N | scored | empty | unparsed | colour agreement | reason names field |")
        print("|---|---|---|---|---|---|")
        print("| %d | %d | %d | %d | %d, %.1f percent | %d, %.1f percent |"
              % (n_total, n_scored, n_empty, n_unparsed, colour_ok, c_rate, reason_ok, r_rate))
        print()
        print("| rule \\ model | " + " | ".join(COLOURS) + " |")
        print("|---|---|---|---|")
        for r in COLOURS:
            print("| %s | " % r + " | ".join(str(confusion[r][m]) for m in COLOURS) + " |")
    else:
        print("N %d, scored %d, empty %d, unparsed %d" % (n_total, n_scored, n_empty, n_unparsed))
        print("colour agreement   %d of %d, %.1f percent" % (colour_ok, n_scored, c_rate))
        print("reason names field %d of %d, %.1f percent" % (reason_ok, n_scored, r_rate))
        print("rule \\ model".ljust(14) + "".join(m.rjust(9) for m in COLOURS))
        for r in COLOURS:
            print(r.ljust(14) + "".join(str(confusion[r][m]).rjust(9) for m in COLOURS))
    if disagreements:
        print()
        print("rows to read (log line, t_s, rule, model colour, field expected, reason):")
        for item in disagreements:
            print("  %d, %s, %s, %s, %s, %r" % item)
    return 0


if __name__ == "__main__":
    sys.exit(main())
