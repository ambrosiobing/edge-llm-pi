#!/usr/bin/env python3
"""Does each project's generation budget agree across the three places that decide it?

    python3 tools/budget_agree.py             # print the table
    python3 tools/budget_agree.py --strict    # exit 1 on a disagreement OR an unreadable default
    python3 tools/budget_agree.py --ci        # exit 1 on a disagreement only
    python3 tools/budget_agree.py --self-test # exercise the decision table both ways

--ci is what continuous integration runs, and it differs from --strict on purpose. --strict
also fails when this tool cannot read a project's default, which is the right answer for a
person checking the tree by hand and the wrong one for a build: it would stop a push over
missing information rather than over a contradiction. That is Joseph's rule for the e02
figure verifier, set on Monday 19 October 2026, and this gate follows it. An unreadable
default is printed under --ci as a line to look at by hand and does not fail the job.

THERE ARE THREE SOURCES OF TRUTH, NOT TWO, AND THAT IS THE WHOLE POINT OF THIS FILE.

    the binary's own default   what runs when nobody passes the option
    the systemd unit           what the deployed image runs
    the wiring sheet           what a person pastes on the bench

A sheet that passes no budget is a defect only when the binary's default disagrees with the
unit. A sheet that passes none while the binary already defaults to the unit's value is simply
relying on a correct default, which is fine.

WHY THIS FILE EXISTS, WHICH IS A LIST OF MY OWN WRONG ANSWERS ON MONDAY 19 OCTOBER 2026. The
comparison was written as a shell one-liner three times and was wrong every time, each time
confidently and each time in the direction of inventing a defect rather than missing one.

  1. `grep -o -- "--n-predict [0-9]*" | head -1` on e15's unit returned 10, out of that unit's
     own header comment saying an earlier draft "named the remedy, --n-predict 10, and then
     shipped 8 anyway". It matched prose about the option instead of the option. Comments are
     stripped here.

  2. `ls .../edgellm-e17/files/*.service | head -1` returned edgellm-e17-sampler.service, which
     has no budget because it is a sampler and not the model loop, and e17 was reported as a
     defect on the strength of the wrong file. Every service file is read here and the values
     collected as a set.

  3. With both of those fixed it still reported e07, e13 and e19 as defects, because it compared
     sheet against unit and assumed every binary defaults to 8. Three of them do, in the shared
     C core, the shared Python loop and e09's own C++ main. The other three set 64 in their own
     main, e13's in Rust, and never needed the option in the sheet at all. That claim reached a
     pushed commit message before it was checked.

The real defects it was written for, all now closed: e02 at a default of 8 against a unit of 12,
and e09, e10 and e15 at 8 against units of 16. Those four sheets passed nothing and therefore
ran the number that truncated 949 objects out of 1000.

WHAT THE DEFAULT COLUMN IS, AND WHAT IT IS NOT. It is the first n_predict assignment found by an
alphabetical walk of the project's sources, falling back to the shared core for that project's
language. That is indicative and not authoritative provenance: for e02 it names
variant_a_cpp.cpp, which is the comparison binary's own budget rather than the one edgellm-e02
runs, and for e18 it names a test file. Both happen to carry the same number as the real default,
which is luck and not a reason to trust the column.

The verdict does not rest on it except where a sheet is silent, which is e07, e13 and e19. For
those three the default was read by hand in their own main, 64 in main.cpp, main.rs and
e19_main.c, and each matches its unit. Anywhere the sheet sets the option the default cannot be
reached and the column is decoration. Finding the true default per project would mean knowing
which of several sources is the binary the unit starts, which this tool deliberately does not
try to work out.
"""
import argparse
import io
import os
import re
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(HERE)
OPT = re.compile(r"--n-predict\s+(\d+)")
# A default assignment in any of the four languages this volume uses.
DEF = re.compile(r"n_predict\s*(?:=|:)\s*(\d+)")
SHARED = {
    "cmake": (os.path.join("common", "c", "src", "edgellm.c"), "the shared C core"),
    "python": (os.path.join("common", "python", "edgellm", "edgellm", "loop.py"),
               "the shared Python loop"),
    "cargo": (os.path.join("common", "rust", "edgellm", "src", "engine.rs"),
              "the shared Rust engine"),
}


def lines_of(path):
    with io.open(path, encoding="utf-8", errors="replace") as fh:
        return [ln for ln in fh if not ln.lstrip().startswith(("#", "//", "*", "/*"))]


def option_values(path):
    """Every --n-predict value passed on a non-comment line."""
    out = []
    for ln in lines_of(path):
        out.extend(OPT.findall(ln))
    return sorted({int(v) for v in out})


def default_in(path):
    """The first n_predict default assignment on a non-comment line, or None."""
    for ln in lines_of(path):
        m = DEF.search(ln)
        if m:
            return int(m.group(1))
    return None


def language_of(proj):
    for name, key in (("Cargo.toml", "cargo"), ("pyproject.toml", "python"),
                      ("CMakeLists.txt", "cmake")):
        if os.path.isfile(os.path.join(proj, name)):
            return key
    return None


def own_default(proj):
    """(value, where) for the project's own default, falling back to its shared core."""
    skip = {"evidence", "replay", "tests", "target", "build", "__pycache__", "firmware"}
    for here, dirs, files in os.walk(proj):
        dirs[:] = [d for d in dirs if d not in skip]
        for name in sorted(files):
            if not name.endswith((".c", ".cpp", ".rs", ".py")):
                continue
            v = default_in(os.path.join(here, name))
            if v is not None:
                return v, "its own " + name
    lang = language_of(proj)
    if lang and lang in SHARED:
        rel, label = SHARED[lang]
        v = default_in(os.path.join(ROOT, rel))
        if v is not None:
            return v, label
    return None, "not found"


def units_of(slug):
    app = os.path.join(ROOT, "meta-edgellm", "recipes-app",
                       "edgellm-" + slug.split("-")[0], "files")
    if not os.path.isdir(app):
        return []
    return [os.path.join(app, n) for n in sorted(os.listdir(app)) if n.endswith(".service")]


def verdict_for(sheet, units, dflt):
    """The decision table, kept apart from the file reading so --self-test can drive it.

    Returns (text, kind) with kind one of "ok", "bad", "unknown" or "no-unit". The order of
    the branches is load bearing and is the thing the self-test below pins: the sheet is
    consulted first, because where the sheet passes the option the default cannot be reached
    and an unreadable default is not a finding.
    """
    if not units:
        return "no unit budget", "no-unit"
    if sheet:
        if sheet != units:
            return "SHEET DISAGREES WITH UNIT", "bad"
        return "sheet sets it and agrees", "ok"
    if dflt is None:
        return "SHEET SILENT and default unreadable, look by hand", "unknown"
    if dflt not in units:
        return "SHEET MISSING and default %d is not the unit's" % dflt, "bad"
    return "sheet relies on the default, which agrees", "ok"


# (sheet, units, default, expected kind, what the case stands for). Six cases covering both
# directions, because a check nobody has watched fail is not known to be a check, and because
# the fourth case here is precisely the one the first version of this comparison got wrong:
# it compared sheet against unit, assumed every binary defaults to 8, and reported e07, e13
# and e19 as defects when all three set 64 in their own main and needed no option at all.
SELF_TEST_CASES = (
    ([12], [12], 8, "ok",
     "the sheet passes the unit's budget, so the default cannot be reached"),
    ([8], [12], 8, "bad",
     "the sheet passes a budget the unit does not, which was e02's defect"),
    ([], [12], 8, "bad",
     "a silent sheet whose default is below the unit, which was e09, e10 and e15"),
    ([], [64], 64, "ok",
     "a silent sheet whose default already agrees, which was e07, e13 and e19"),
    ([], [16], None, "unknown",
     "a silent sheet and an unreadable default, a look by hand and not a build failure"),
    ([], [], None, "no-unit",
     "a project whose units pass no budget at all"),
)


def self_test():
    failures = 0
    for sheet, units, dflt, want, what in SELF_TEST_CASES:
        _, kind = verdict_for(sheet, units, dflt)
        if kind != want:
            failures += 1
        print("%-4s want %-8s got %-8s  %s"
              % ("pass" if kind == want else "FAIL", want, kind, what))
    print("budget_agree --self-test: %d of %d failed" % (failures, len(SELF_TEST_CASES)))
    return 1 if failures else 0


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--strict", action="store_true")
    ap.add_argument("--ci", action="store_true",
                    help="exit 1 on a real disagreement only, never on an unreadable default")
    ap.add_argument("--self-test", action="store_true",
                    help="run the decision table against fabricated cases and exit")
    args = ap.parse_args()

    if args.self_test:
        return self_test()

    projects = os.path.join(ROOT, "projects")
    if not os.path.isdir(projects):
        print("budget_agree: no projects/ under %s" % ROOT, file=sys.stderr)
        return 2

    print("%-26s %-8s %-7s %-22s %s"
          % ("PROJECT", "DEFAULT?", "UNIT", "FIRST FOUND IN", "VERDICT"))
    print("(DEFAULT? is indicative only; see the header. The verdict rests on it only where")
    print(" the sheet is silent, and those were confirmed by hand.)")
    bad = 0
    unknown = 0
    for slug in sorted(os.listdir(projects)):
        proj = os.path.join(projects, slug)
        sheet_path = os.path.join(proj, "wiring.md")
        if not os.path.isfile(sheet_path):
            continue
        sheet = option_values(sheet_path)
        units = sorted({v for u in units_of(slug) for v in option_values(u)})
        dflt, where = own_default(proj)

        verdict, kind = verdict_for(sheet, units, dflt)
        if kind == "bad":
            bad += 1
        elif kind == "unknown":
            unknown += 1

        print("%-26s %-8s %-7s %-22s %s"
              % (slug, dflt if dflt is not None else "?",
                 ",".join(str(v) for v in units) or "none", where, verdict))

    print()
    print("%d real disagreement(s), %d default(s) this tool could not read" % (bad, unknown))
    if bad:
        print("A bench run at a default below the unit's budget is caught by")
        print("tools/agreement.py, which returns non-zero on any unclosed object, so the cost")
        print("is the sitting and not the figure.")
    if args.ci and unknown:
        print("NOT CHECKED: %d default(s) this tool cannot read. Under --ci that is a line to"
              % unknown)
        print("look at by hand and not a build failure; --strict fails on it.")
    if args.ci:
        return 1 if bad else 0
    return 1 if ((bad or unknown) and args.strict) else 0


if __name__ == "__main__":
    sys.exit(main())
