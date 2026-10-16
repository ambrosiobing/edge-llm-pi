#!/usr/bin/env python3
"""The acceptance instrument of e08: does every answer carry the live reading?

    python projects/e08-explorer700-console/tools/answer_check.py <log.csv> [--strict]

Reads the CSV common/c/src/elog.c writes. The raw column holds the event's inputs joined
by ';' in the console's layout, question;rtc_s;pi_s;pressure_kpa;temperature_c, and the
model column holds the JSON object {"answer": "..."} the grammar admits. For each row the
quantity the question asked for is looked for among the numbers in the answer, and the
row passes when one of them lies within 1 percent of the logged input (or within 0.05
of it, whichever is wider, so that a difference of 0.0 s can be matched at all):

    0  what is the pressure trend     the pressure, in kPa or as hPa (ten times the kPa)
    1  is the clock drifting          the difference DS3231 minus Pi in seconds, by size
    2  how warm is the box            the temperature in C
    3  what time is it by the RTC     the RTC time as HH:MM in UTC, matched as text
    4  summarise the last hour        the pressure and the temperature, both

Rows with an empty model column (the daemon was away past --retry-max, or answered
ERROR) count as failures and are listed. The stub's answer "." carries no number, so a
CI log scores 0 of N here by construction; this tool is for a bench log. Exit 1 with
--strict when any row fails, else 0; the summary line is printed either way.
"""
import csv
import json
import re
import sys
import time

NUMBER = re.compile(r"[-+]?\d+(?:\.\d+)?")
HHMM = re.compile(r"\b(\d{2}):(\d{2})\b")


def close(value, target):
    tolerance = max(0.01 * abs(target), 0.05)
    return abs(value - target) <= tolerance


def numbers_in(text):
    return [float(m.group(0)) for m in NUMBER.finditer(text)]


def answer_of(cell):
    cell = cell.strip()
    if not cell:
        return None
    try:
        value = json.loads(cell)
    except ValueError:
        return cell
    if isinstance(value, dict) and "answer" in value:
        return str(value["answer"])
    return cell


def check_row(raw, answer):
    """Returns (ok, reason)."""
    parts = raw.split(";")
    if len(parts) < 5:
        return False, "raw column has %d values, needs 5" % len(parts)
    q = int(float(parts[0]))
    rtc_s, pi_s, p_kpa, t_c = (float(x) for x in parts[1:5])
    nums = numbers_in(answer)
    has_pressure = any(close(n, p_kpa) or close(n, 10.0 * p_kpa) for n in nums)
    has_temperature = any(close(n, t_c) for n in nums)
    drift = abs(rtc_s - pi_s)
    has_drift = any(close(abs(n), drift) for n in nums)
    hhmm = time.strftime("%H:%M", time.gmtime(rtc_s))
    has_time = hhmm in answer
    if q == 0:
        return has_pressure, "pressure %.2f kPa not in the answer" % p_kpa
    if q == 1:
        return has_drift, "difference %.1f s not in the answer" % drift
    if q == 2:
        return has_temperature, "temperature %.1f C not in the answer" % t_c
    if q == 3:
        return has_time, "RTC time %s UTC not in the answer" % hhmm
    if q == 4:
        return (has_pressure and has_temperature,
                "pressure %.2f kPa and temperature %.1f C not both in the answer" % (p_kpa, t_c))
    return False, "question index %d outside 0 to 4" % q


def main(argv):
    if not argv or len(argv) > 2:
        print(__doc__.strip().splitlines()[2])
        return 2
    strict = "--strict" in argv
    path = [a for a in argv if a != "--strict"][0]
    with open(path, newline="", encoding="utf-8") as fh:
        reader = csv.DictReader(fh)
        if reader.fieldnames is None or "raw" not in reader.fieldnames or \
                "model" not in reader.fieldnames:
            print("answer_check: %s lacks raw and model columns" % path)
            return 2
        rows = list(reader)
    passed = 0
    failures = []
    for number, row in enumerate(rows, 2):
        answer = answer_of(row["model"])
        if answer is None:
            failures.append((number, "no model answer (daemon away or ERROR)"))
            continue
        ok, reason = check_row(row["raw"], answer)
        if ok:
            passed += 1
        else:
            failures.append((number, reason + ": " + answer[:80]))
    print("answer_check: %d of %d answers carry the live reading within 1 percent"
          % (passed, len(rows)))
    for number, reason in failures:
        print("  line %d: %s" % (number, reason))
    engines = sorted({r.get("engine", "") for r in rows})
    print("engine column: " + ", ".join(engines))
    return 1 if strict and failures else 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
