#!/usr/bin/env python3
"""The acceptance numbers of project e17, read from one event log.

    python projects/e17-cellular-summariser/tools/hallucination_check.py <log.csv>
    python projects/e17-cellular-summariser/tools/hallucination_check.py <log.csv> --md
    ... --operators "A1,Magenta,Drei,spusu"      extra names to recognise

Reads the CSV common/c/src/elog.c writes (columns t_s, tag, raw, rule, model, ...).
The model column is the JSON object of grammars/e17_status.gbnf, {"state": ...,
"report": ...}; tools/agreement.py reads a "decision" key and so does not apply to
this project, which is why this checker also prints the agreement table. Rows whose
model column is empty are the 14 samples in every 15 that the loop logs with the rule
alone (project_format_prompt() returns NULL for them); they are counted separately and
never as agreement.

Three numbers come out, and the chapter publishes all three whatever they are.

  state agreement      rows whose "state" equals the rule column, over the rows scored
  invented operators   names in the report that the modem did not report in that row
  numbers not given    numbers in the report that were not in that row's inputs

What counts as an invented operator name: any name in the universe of operator names,
which is every distinct tag in this log plus the names passed with --operators, that
appears in the report as a word while it is not that row's own tag. That is the strict
count, and it is the published one: it needs no judgement and it cannot be argued
with, because the name was reported by the modem in some other minute and not in this
one.

What counts as a number not given: a digit run in the report, not touching a letter
(so "Cat-M1" and "NB-IoT" carry none), whose value is not one of that row's five input
numbers and is not 10, the threshold the system prompt states. The signal quality, the
bit error rate, the registration state, the access technology and the power saving
mode are the five. Two numbers are allowed beside them because they are properties of
the command rather than readings: 10, the threshold, and 31, the top of the CSQ scale,
so that a report saying "signal 9 of 31" is not counted against the model.

Beside the strict count the checker prints a loose one, "unrecognised capitalised
words": capitalised words in the report that are neither in the operator universe nor
in the small stop list below. It is advisory. A model writing "Vienna" would be caught
by it and by nothing else, and a model writing "Registration" would be caught by it
wrongly, which is exactly why the published number is the strict one.
"""
import argparse
import csv
import json
import re
import sys

STATES = ("OK", "DEGRADED", "DOWN")
THRESHOLD = 10                      # the one number the system prompt itself states
CSQ_SCALE_MAX = 31                  # the top of the AT+CSQ scale, not a reading
NUMBER = re.compile(r"(?<![A-Za-z0-9])(\d+)(?![A-Za-z0-9])")
WORD = re.compile(r"[A-Za-z][A-Za-z0-9]*")
STOP_WORDS = {
    "a", "and", "at", "access", "an", "are", "band", "bit", "by", "cat", "cell",
    "degraded", "down", "error", "for", "from", "has", "home", "in", "is", "it", "iot",
    "its", "last", "lower", "m1", "minute", "mode", "modem", "network", "nb", "no",
    "none", "not", "of", "off", "on", "ok", "operator", "pi", "power", "quality", "radio",
    "rate", "raspberry", "registered", "registration", "reported", "roaming", "saving",
    "searching", "sentence", "signal", "state", "status", "strong", "technology",
    "the", "this", "to", "unknown", "weak", "while", "with",
}


def parse_model(text):
    """Returns (state, report, status) with status one of ok, empty, unparsed."""
    text = text.strip()
    if not text:
        return None, None, "empty"
    try:
        value = json.loads(text)
    except ValueError:
        return None, None, "unparsed"
    if not isinstance(value, dict) or "state" not in value or "report" not in value:
        return None, None, "unparsed"
    return str(value["state"]), str(value["report"]), "ok"


def input_numbers(raw):
    """The row's five input numbers, from the log's raw column (v0;v1;...)."""
    out = set()
    for part in raw.split(";"):
        part = part.strip()
        if not part:
            continue
        try:
            out.add(int(float(part)))
        except ValueError:
            continue
    out.add(THRESHOLD)
    out.add(CSQ_SCALE_MAX)
    return out


def load(path):
    with open(path, newline="", encoding="utf-8") as fh:
        reader = csv.DictReader(fh)
        need = ("tag", "raw", "rule", "model")
        if reader.fieldnames is None or any(c not in reader.fieldnames for c in need):
            raise SystemExit("hallucination_check: %s lacks tag, raw, rule and model "
                             "columns" % path)
        return list(reader)


def main():
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("log")
    parser.add_argument("--md", action="store_true", help="print Markdown tables")
    parser.add_argument("--operators", default="",
                        help="comma separated operator names to recognise beside the "
                             "names this log's tag column already carries")
    args = parser.parse_args()

    rows = load(args.log)
    universe = {row["tag"].strip() for row in rows if row["tag"].strip() not in ("", "none")}
    universe |= {name.strip() for name in args.operators.split(",") if name.strip()}
    lowered = {name.lower(): name for name in universe}

    n_total = len(rows)
    n_empty = n_unparsed = 0
    agree = invented = not_given = loose = 0
    confusion = {r: {m: 0 for m in STATES} for r in STATES}
    findings = []
    for number, row in enumerate(rows, 2):
        rule = row["rule"].strip()
        state, report, status = parse_model(row["model"])
        if status == "empty":
            n_empty += 1
            continue
        if status == "unparsed":
            n_unparsed += 1
            continue
        if rule in confusion and state in confusion[rule]:
            confusion[rule][state] += 1
        if state == rule:
            agree += 1
        own = row["tag"].strip()
        allowed = input_numbers(row["raw"])
        bad_names = [lowered[w.lower()] for w in WORD.findall(report)
                     if w.lower() in lowered and w.lower() != own.lower()]
        bad_numbers = [int(m) for m in NUMBER.findall(report) if int(m) not in allowed]
        odd_words = [w for w in WORD.findall(report)
                     if w[:1].isupper() and w.lower() not in STOP_WORDS
                     and w.lower() not in lowered and w.lower() != own.lower()]
        invented += len(bad_names)
        not_given += len(bad_numbers)
        loose += len(odd_words)
        if state != rule or bad_names or bad_numbers or odd_words:
            findings.append((number, row.get("t_s", ""), own, rule, state,
                             ",".join(bad_names) or "-",
                             ",".join(str(v) for v in bad_numbers) or "-",
                             ",".join(odd_words) or "-"))

    n_scored = n_total - n_empty - n_unparsed
    rate = 100.0 * agree / n_scored if n_scored else 0.0

    if args.md:
        print("| N | scored | rule only | unparsed | state agreement | invented "
              "operators | numbers not given | unrecognised words |")
        print("|---|---|---|---|---|---|---|---|")
        print("| %d | %d | %d | %d | %d, %.1f percent | %d | %d | %d |"
              % (n_total, n_scored, n_empty, n_unparsed, agree, rate, invented,
                 not_given, loose))
        print()
        print("| rule \\ model | " + " | ".join(STATES) + " |")
        print("|---|---|---|---|")
        for r in STATES:
            print("| %s | " % r + " | ".join(str(confusion[r][m]) for m in STATES) + " |")
    else:
        print("N %d, scored %d, rule only %d, unparsed %d"
              % (n_total, n_scored, n_empty, n_unparsed))
        print("state agreement     %d of %d, %.1f percent" % (agree, n_scored, rate))
        print("invented operators  %d" % invented)
        print("numbers not given   %d" % not_given)
        print("unrecognised words  %d (advisory)" % loose)
        print("operator universe   %s" % (", ".join(sorted(universe)) or "none"))
        print("rule \\ model".ljust(14) + "".join(m.rjust(10) for m in STATES))
        for r in STATES:
            print(r.ljust(14) + "".join(str(confusion[r][m]).rjust(10) for m in STATES))
    if findings:
        print()
        print("rows to read (log line, t_s, operator, rule, state, invented names, "
              "numbers not given, unrecognised words):")
        for item in findings:
            print("  %s" % ", ".join(str(part) for part in item))
    return 0


if __name__ == "__main__":
    sys.exit(main())
