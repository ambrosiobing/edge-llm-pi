#!/usr/bin/env python3
"""pytest over every Python test suite in the tree, with the known failures named.

    python tools/pytest_all.py            run them all
    python tools/pytest_all.py --list     print what would run, and run nothing

Every pyproject.toml under projects/ and under common/python, which is every one in the
tree. The sibling tools/cargo_all.py does the same for Rust and this file is deliberately
its shape.

WHY THIS IS A SCRIPT AND NOT A LOOP IN THE WORKFLOW, and it is the second time the same
gap has been found. Until Tuesday 20 October 2026 the workflow globbed
`projects/*/pyproject.toml`, which is one level deep and rooted at projects/. The shared
Python package lives at common/python/edgellm, so its four test files, the logger, the
decision loop, the replay source and the stub, were run by no job at all. Four projects
import that package.

That is the same gap cargo_all.py was written for on Saturday 17 October 2026, and its
header says so in as many words: common/rust was added there because "roughly eighty of its
test functions had never been run by any job, which is the same gap as the one-level glob,
one directory over". The Rust half of that lesson was applied and the Python half was not.
A fix that lands in one language and not its neighbour is this repository's commonest
defect and it now has a third instance.

HOW THE SUITES ARE MADE IMPORTABLE, and why there is no pip install here. All three
packages are flat: the package directory sits inside the directory holding its
pyproject.toml, so putting that directory on PYTHONPATH is enough. Nothing in any of the
four suites imports a third-party module; every import is stdlib, pytest, or one of this
repository's own packages, which was checked rather than assumed. So this script needs no
install, cannot mutate the machine it runs on, and behaves the same on the authoring
laptop, on skyhorizon's WSL where `pip install -e` is refused outright by PEP 668, and on
the runner. If a suite ever does need a third-party module, install it in the job and say
so in the step rather than here.
"""
import argparse
import os
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent

# Same discipline as cargo_all.py: an entry needs a reason and a date, and an entry that
# starts passing fails this script, so the list cannot quietly become a set of excuses.
KNOWN = {}

# Where Python packages live in this tree.
WALK = ("projects", "common/python")

# The shared package is on the path for every suite, because e15 and e20 import it: their
# pyproject files say a PEP 508 dependency cannot name a relative path, so the dependency
# is by name and something has to supply it.
SHARED = "common/python/edgellm"

SKIP_PARTS = (".venv", "build", "dist", "target", "__pycache__")


def suites():
    """[(manifest dir, tests dir)] for every package in the tree that has tests."""
    found = []
    for top in WALK:
        base = ROOT / top
        if not base.is_dir():
            print("pytest_all: %s is not a directory, which cannot be right" % top)
            continue
        for p in sorted(base.rglob("pyproject.toml")):
            rel = p.parent.relative_to(ROOT).as_posix()
            if any(("/" + part + "/") in ("/" + rel + "/") for part in SKIP_PARTS):
                continue
            if len(rel.split("/")) > 3:
                continue
            tests = p.parent / "tests"
            found.append((rel, tests.is_dir()))
    return found


def main(argv):
    ap = argparse.ArgumentParser()
    ap.add_argument("--list", action="store_true", help="print the suites and exit")
    args = ap.parse_args(argv)

    found = suites()
    if not found:
        print("pytest_all: no pyproject.toml under %s, which cannot be right"
              % " or ".join(WALK))
        return 1
    if not (ROOT / SHARED / "pyproject.toml").is_file():
        print("pytest_all: %s has no pyproject.toml; this script's path setup is wrong"
              % SHARED)
        return 1

    if args.list:
        for rel, has_tests in found:
            mark = "  (known failure)" if rel in KNOWN else ""
            if not has_tests:
                mark += "  (no tests/ directory, nothing to run)"
            print("%s%s" % (rel, mark))
        print("pytest_all: %d packages, %d with tests, %d known failures"
              % (len(found), sum(1 for _, t in found if t), len(KNOWN)))
        stale = [k for k in KNOWN if k not in [r for r, _ in found]]
        for k in stale:
            print("STALE: KNOWN names %s, which is not in the tree" % k)
        return 1 if stale else 0

    problems = 0
    ran = 0
    for rel, has_tests in found:
        if not has_tests:
            print("\n== %s has no tests/ directory, nothing to run" % rel)
            continue
        print("\n== %s" % rel)
        env = dict(os.environ)
        path = [str(ROOT / SHARED), str(ROOT / rel)]
        if env.get("PYTHONPATH"):
            path.append(env["PYTHONPATH"])
        env["PYTHONPATH"] = os.pathsep.join(path)
        r = subprocess.run([sys.executable, "-m", "pytest", rel + "/tests"],
                           cwd=str(ROOT), env=env)
        ran += 1
        if rel in KNOWN:
            if r.returncode == 0:
                print("FAIL %s passed and is listed as a known failure.\n"
                      "     Remove it from KNOWN in tools/pytest_all.py." % rel)
                problems += 1
            else:
                print("known failure, allowed: %s" % KNOWN[rel])
        elif r.returncode != 0:
            print("FAIL %s" % rel)
            problems += 1

    print("\npytest_all: %d packages, %d suites run, %d known failures, %d problems"
          % (len(found), ran, len(KNOWN), problems))
    return 1 if problems else 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
