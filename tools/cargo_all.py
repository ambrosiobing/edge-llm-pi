#!/usr/bin/env python3
"""cargo test over every Rust manifest in the tree, with the known failures named.

    python tools/cargo_all.py            run them all
    python tools/cargo_all.py --list     print what would run, and run nothing

Every Cargo.toml under projects/ and under common/rust/, which is every one in the tree.

Why this is a script and not four lines in the workflow. Until Saturday 17 October 2026 the
workflow globbed `projects/*/Cargo.toml`, which is one level deep, and e18 keeps its halves
in `rust/` and `c/`. So nothing ever compiled e18, and because nothing compiled it, its
dependency on the shared crate could name four cargo features that crate does not declare
and no run said so. The glob is now a walk, and the walk lives here so that it can be read
and run on a laptop rather than only by the runner.

KNOWN is the other half of that lesson, and it is empty as of Sunday 18 October 2026. It
held one entry for one day. e18's Rust half did not compile: it had been written against an
interface the shared crate never had, eight errors, of which two were decisions rather than
repairs. Both were made that day and the entry came out the same evening, not because
someone remembered to tidy it but because **a KNOWN entry that starts passing fails this
script**, and it did, naming the two places to edit. That is the whole reason the mechanism
is shaped this way: a list of excuses nobody has to maintain is the thing that rots. Add an
entry only with the reason and the date, and expect the script to take it away from you.
"""
import argparse
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent

KNOWN = {}


# Where Rust lives in this tree. common/rust is here because nothing ran the shared crate's
# own tests until Saturday 17 October 2026: the workflow named no path under common/, and
# this script's first version walked projects/ only. Four projects depend on that crate and
# roughly eighty of its test functions had never been run by any job, which is the same gap
# as the one-level glob, one directory over.
WALK = ("projects", "common/rust")


def manifests():
    found = []
    for top in WALK:
        base = ROOT / top
        if not base.is_dir():
            print("cargo_all: %s is not a directory, which cannot be right" % top)
            continue
        for p in sorted(base.rglob("Cargo.toml")):
            rel = p.relative_to(ROOT).as_posix()
            if "/target/" in "/" + rel:
                continue
            if len(rel.split("/")) > 4:
                continue
            found.append(rel)
    return found


def main(argv):
    ap = argparse.ArgumentParser()
    ap.add_argument("--list", action="store_true", help="print the manifests and exit")
    args = ap.parse_args(argv)

    found = manifests()
    if not found:
        print("cargo_all: no Cargo.toml under projects/, which cannot be right")
        return 1

    if args.list:
        for rel in found:
            mark = "  (known failure)" if rel in KNOWN else ""
            print("%s%s" % (rel, mark))
        print("cargo_all: %d manifests, %d known failures" % (len(found), len(KNOWN)))
        stale = [k for k in KNOWN if k not in found]
        for k in stale:
            print("STALE: KNOWN names %s, which is not in the tree" % k)
        return 1 if stale else 0

    problems = 0
    for rel in found:
        print("\n== %s" % rel)
        r = subprocess.run(["cargo", "test", "--manifest-path", rel], cwd=str(ROOT))
        if rel in KNOWN:
            if r.returncode == 0:
                print("FAIL %s passed and is listed as a known failure.\n"
                      "     Remove it from KNOWN in tools/cargo_all.py and from the\n"
                      "     blocked_on note in its project.toml." % rel)
                problems += 1
            else:
                print("known failure, allowed: %s" % KNOWN[rel])
        elif r.returncode != 0:
            print("FAIL %s" % rel)
            problems += 1

    print("\ncargo_all: %d manifests, %d known failures, %d problems"
          % (len(found), len(KNOWN), problems))
    return 1 if problems else 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
