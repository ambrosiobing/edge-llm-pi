#!/usr/bin/env python3
"""Measure what one fsync costs on the filesystem a log will actually be written to.

    python3 tools/fsync_cost.py [DIR] [--rows N] [--interval MS]

DIR defaults to the current directory and decides which device is measured, which is the whole
point: the answer on an SD card and the answer on a tmpfs are different, and only one of them
is the answer. --interval is the per-decision wall time the cost is reported as a fraction of,
defaulting to 2827 ms, which is e02's restore pass at 1325.6 ms of prompt and 1524.8 ms of
generation less rounding.

WHY A CONTROL RUN. The quantity wanted is the cost of fsync, not the cost of writing a row, so
the same loop runs twice, once with flush alone and once with flush and fsync, and the
difference is reported. A single timed loop with fsync in it measures the row and the sync
together and cannot separate them.

WHY THIS IS NOT A THEORETICAL NUMBER. common/c/src/elog.c calls fsync after every row from
Monday 19 October 2026, so that a board which restarts mid-run does not take finished decisions
with it. The decision was Joseph's and came with an instruction: measure the cost rather than
assume it. On Saturday 17 October 2026 the absence of that call cost about five finished rows
out of a run of a thousand, and the eighty-five minute agreement run is the one that must not
lose any.

WHAT IT REFUSES TO REPORT. A directory on tmpfs, because fsync there returns without touching
a device and the measurement would read as free. The mount is read from /proc/mounts and named
in the output, so the number always arrives with the filesystem it belongs to.
"""
import argparse
import io
import os
import sys
import time


def mount_for(path):
    """The longest mount point containing path, with its filesystem type."""
    best = ("", "")
    try:
        with io.open("/proc/mounts", encoding="utf-8") as fh:
            for line in fh:
                parts = line.split()
                if len(parts) >= 3 and path.startswith(parts[1]) and len(parts[1]) > len(best[0]):
                    best = (parts[1], parts[2])
    except OSError:
        return ("unknown", "unknown")
    return best if best[0] else ("unknown", "unknown")


def timed(path, rows, sync):
    """Mean milliseconds per row for the write-and-flush loop, with or without the sync."""
    fh = open(path, "w")
    try:
        start = time.perf_counter()
        for i in range(rows):
            fh.write("%d,synthetic,NORMAL,ALERT,1325.6,1524.8\n" % i)
            fh.flush()
            if sync:
                os.fsync(fh.fileno())
        end = time.perf_counter()
    finally:
        fh.close()
        try:
            os.remove(path)
        except OSError:
            pass
    return 1000.0 * (end - start) / rows


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("directory", nargs="?", default=".")
    ap.add_argument("--rows", type=int, default=200)
    ap.add_argument("--interval", type=float, default=2827.0,
                    help="per-decision wall time in ms the cost is a fraction of")
    args = ap.parse_args()

    target = os.path.abspath(args.directory)
    if not os.path.isdir(target):
        print("fsync_cost: %s is not a directory" % target, file=sys.stderr)
        return 2
    point, kind = mount_for(target)
    print("directory   %s" % target)
    print("filesystem  %s on %s" % (kind, point))
    if kind in ("tmpfs", "ramfs"):
        print("fsync_cost: %s is %s, where fsync never reaches a device. The number would read"
              % (point, kind), file=sys.stderr)
        print("fsync_cost: as free and would be wrong. Point this at the card instead.",
              file=sys.stderr)
        return 2

    probe = os.path.join(target, ".fsync_cost_probe.tmp")
    if os.path.exists(probe):
        print("fsync_cost: %s exists; refusing to overwrite" % probe, file=sys.stderr)
        return 2

    # Warm the path once and throw the result away, so the first run does not carry the cost
    # of creating the file for the first time on this filesystem.
    timed(probe, min(20, args.rows), True)

    plain = timed(probe, args.rows, False)
    synced = timed(probe, args.rows, True)
    cost = synced - plain

    print("rows        %d per loop" % args.rows)
    print("flush only  %7.3f ms per row" % plain)
    print("flush+fsync %7.3f ms per row" % synced)
    print("fsync costs %7.3f ms per row" % cost)
    print("against a decision of %.0f ms that is %.4f percent"
          % (args.interval, 100.0 * cost / args.interval))
    print("over 1000 decisions it adds %.1f s to a run of %.0f s"
          % (1000.0 * cost / 1000.0, args.interval))
    return 0


if __name__ == "__main__":
    sys.exit(main())
