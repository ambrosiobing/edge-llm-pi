#!/usr/bin/env python3
"""Say whether a log from a board actually holds what its size claims.

    python3 tools/log_tail.py <log.csv> [<log.csv> ...]

Exit status is 1 if any file is truncated, so this can gate a figure rather than only inform
one. Use it on every log from a run that did not end cleanly, before counting its rows.

WHY. A board that restarts mid-write leaves a file whose size was recorded and whose tail was
never written to the card, so the tail reads back as NUL. On Saturday 17 October 2026
projects/e02-decision-loop/evidence/e02_run_npredict12.csv came off the bench Raspberry Pi 4
Model B that way: 13645 bytes, of which the last 562 are NUL, holding 117 usable rows.

Two tools disagree about such a file and only one of them is right:

    grep -c ""   reported 680     wrong, it splits the NUL run into lines of its own
    wc -l        reported 118     right, that is the newline count
    this script  reports 117      the CSV records that are actually there

A per-row flush in the writing program does not prevent this. A flush moves bytes to the
kernel, not to the card; only fsync does that. So a partial log is evidence of how far a run
got and not of what it measured, and the rows after the first NUL are absent no matter what
the file size says.
"""
import csv
import io
import os
import sys


def inspect(path):
    with io.open(path, "rb") as fh:
        b = fh.read()
    cut = b.find(b"\x00")
    good = b if cut < 0 else b[:cut]
    try:
        records = len(list(csv.DictReader(io.StringIO(good.decode("utf-8")))))
    except UnicodeDecodeError:
        records = -1
    return {
        "bytes": len(b),
        "newlines": b.count(b"\n"),
        "nuls": b.count(b"\x00"),
        "first_nul": cut,
        "records": records,
        "ends_on_newline": good.endswith(b"\n") if good else False,
        "text_after_nul": len(b[cut:].strip(b"\x00")) if cut >= 0 else 0,
    }


def main(argv):
    if not argv:
        print(__doc__.strip().splitlines()[2].strip(), file=sys.stderr)
        print("usage: log_tail.py <log.csv> [<log.csv> ...]", file=sys.stderr)
        return 2
    truncated = 0
    for path in argv:
        if not os.path.exists(path):
            print("%s: no such file" % path, file=sys.stderr)
            return 2
        d = inspect(path)
        name = os.path.basename(path)
        if d["first_nul"] < 0:
            print("%-52s INTACT     %6d bytes, %4d rows"
                  % (name, d["bytes"], d["records"]))
            continue
        truncated += 1
        print("%-52s TRUNCATED  %6d bytes, %4d rows usable"
              % (name, d["bytes"], d["records"]))
        print("%-52s            %d NUL bytes from offset %d, %.1f percent of the file"
              % ("", d["nuls"], d["first_nul"],
                 100.0 * (d["bytes"] - d["first_nul"]) / d["bytes"]))
        print("%-52s            last kept row is complete: %s"
              % ("", d["ends_on_newline"]))
        if d["text_after_nul"]:
            print("%-52s            WARNING %d non-NUL bytes sit after the NUL run, so this"
                  % ("", d["text_after_nul"]))
            print("%-52s            is not a simple truncation; read the file by hand"
                  % "")
    if truncated:
        print()
        print("%d of %d file(s) truncated. Rows after the first NUL are absent whatever the"
              % (truncated, len(argv)))
        print("size says, so do not count them and do not publish a figure that includes them.")
    return 1 if truncated else 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
