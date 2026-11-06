#!/usr/bin/env python3
"""Generate e20's replay files and its oracle. Python is allowed here as a generator.

    python projects/e20-scorecard/tools/make_replay.py

Three files are written, and all three are dated records of one run of this script:

    replay/scorecard_snapshot.csv   the twenty-row scorecard as the repository held it
                                    when this ran; eleven columns, no cell empty
    replay/scorecards_50.csv        fifty presentations of that snapshot: t_s, tag, the
                                    seed that reshuffles the twenty rows, and the three
                                    one-based identifier numbers the rule answers
    tests/oracle/e20.expected.csv   the fifty rule answers and, in the model column,
                                    what the CI stub answers for every row

The fifty rows differ in one thing only, the order the twenty rows are shown in. The
rule cannot see that order, so its answer is the same on all fifty; the acceptance test
is the share of the fifty on which the model's first identifier is the rule's, which is
the share of the fifty on which the model's answer did not depend on the presentation
either.

The snapshot exists so that the replay and the oracle do not move when a nineteenth
chapter lands its software: a replay run is scored against the snapshot, and only
`main.py --live` ranks the repository as it is now. Regenerate all three together, and
the chapter's scorecard table with them, whenever a project gains evidence.

The stub's answer is not hardcoded here: it is the first expansion of
grammars/e20_next.gbnf under the rule of common/c/src/edgellm_stub.c, computed by
edgellm_e20.remote.first_expansion, so a change to the grammar moves the oracle with
it instead of leaving it wrong.

Written Wednesday 14 October 2026.
"""
import sys
from pathlib import Path

HERE = Path(__file__).resolve().parent
PROJECT = HERE.parent
REPO = PROJECT.parent.parent
sys.path.insert(0, str(PROJECT))

from edgellm_e20 import rank                                 # noqa: E402
from edgellm_e20.main import parse_answer                     # noqa: E402
from edgellm_e20.remote import first_expansion                # noqa: E402
from edgellm_e20.scorecard import (IDS, ScorecardError, as_snapshot_csv,  # noqa: E402
                                   blank_cells, build, notes_of)

SNAPSHOT = PROJECT / "replay" / "scorecard_snapshot.csv"
PROVENANCE = PROJECT / "replay" / "PROVENANCE.txt"
REPLAY = PROJECT / "replay" / "scorecards_50.csv"
ORACLE = REPO / "tests" / "oracle" / "e20.expected.csv"
GRAMMAR = PROJECT / "grammar.gbnf"
N = 50
TAG = "scorecard"
FIRST_SEED = 2026


def house_date(when):
    """Friday 6 November 2026, 15:40. The volume's date form, with no leading zero on
    the day, which strftime cannot give portably across Linux and Windows."""
    return "%s %d %s %d, %02d:%02d" % (when.strftime("%A"), when.day,
                                       when.strftime("%B"), when.year,
                                       when.hour, when.minute)


def provenance_text(when, head, measured, total):
    """The one fact the three generated files do not carry: WHEN they were generated.

    The snapshot is a frozen picture of a changing thing, which is the whole reason it
    exists, and its neighbours in replay/ are fixtures whose content never moves. So a
    stale fixture is impossible and a stale snapshot is invisible, and on Friday 6
    November 2026 it had been stale for eight days across four projects without anything
    saying so. Comparing the line below against the newest file in any evidence/ directory
    is what makes that visible, and it is why this file is written beside the other three
    rather than left to the record that happens to mention a regeneration.
    """
    template = """generated   %s
tree        %s
rows        %d of %d carry numbers, %d read not measured
covers      replay/scorecard_snapshot.csv, replay/scorecards_50.csv,
            tests/oracle/e20.expected.csv
stale when  any projects/*/evidence/ holds a record newer than the date above;
            the generator's own instruction is to regenerate all three
            together whenever a project gains evidence
"""
    return template % (house_date(when), head, measured, total, total - measured)


def git_head():
    """The tree the snapshot was taken from, or a stated absence rather than a guess."""
    try:
        import subprocess
        out = subprocess.run(["git", "rev-parse", "--short", "HEAD"], cwd=str(REPO),
                             capture_output=True, text=True, timeout=10)
        return out.stdout.strip() or "unknown: git gave no answer"
    except Exception:
        return "unknown: git not available here"


def main():
    try:
        rows = build(REPO)
    except ScorecardError as exc:
        print("make_replay: %s" % exc, file=sys.stderr)
        return 1
    empty = blank_cells(rows)
    if empty:
        print("make_replay: the scorecard has empty cells: %s" % ", ".join(empty),
              file=sys.stderr)
        return 1

    three = [row.project for row in rank.top(rows, 3)]
    numbers = [IDS.index(project) + 1 for project in three]
    stub_text = first_expansion(GRAMMAR.read_text(encoding="ascii"))
    stub_picks, _ = parse_answer(stub_text)
    stub_pick = stub_picks[0] if stub_picks else ""

    SNAPSHOT.parent.mkdir(parents=True, exist_ok=True)
    ORACLE.parent.mkdir(parents=True, exist_ok=True)
    with SNAPSHOT.open("w", newline="", encoding="ascii") as handle:
        as_snapshot_csv(rows, handle)

    matched = 0
    with REPLAY.open("w", newline="\n", encoding="ascii") as f_rep, \
            ORACLE.open("w", newline="\n", encoding="ascii") as f_orc:
        f_rep.write("t_s,tag,v0,v1,v2,v3\n")
        f_orc.write("t_s,tag,rule,model\n")
        for index in range(N):
            seed = FIRST_SEED + index
            f_rep.write("%.1f,%s,%d,%d,%d,%d\n"
                        % (float(index), TAG, seed, numbers[0], numbers[1], numbers[2]))
            f_orc.write('%.6f,%s,%s,"%s"\n' % (float(index), TAG, three[0], stub_pick))
            matched += 1 if stub_pick == three[0] else 0

    measured = sum(1 for row in rows if row.measured)
    import datetime
    PROVENANCE.write_text(provenance_text(datetime.datetime.now(), git_head(),
                                          measured, len(rows)), encoding="ascii")
    print("wrote %s" % SNAPSHOT.relative_to(REPO).as_posix())
    print("wrote %s" % REPLAY.relative_to(REPO).as_posix())
    print("wrote %s" % PROVENANCE.relative_to(REPO).as_posix())
    print("wrote %s" % ORACLE.relative_to(REPO).as_posix())
    print("the scorecard: %d of %d rows carry numbers, %d read not measured"
          % (measured, len(rows), len(rows) - measured))
    print("the rule's three: %s" % ", ".join(three))
    print("the stub's answer: %s" % (stub_text or "(empty)"))
    print("the stub's first identifier: %s" % (stub_pick or "(none)"))
    print("stub agreement on the top pick: %d of %d, %.1f percent"
          % (matched, N, 100.0 * matched / N))
    notes = notes_of(rows)
    print("rows carrying a note: %d of %d" % (len(notes), len(rows)))
    return 0


if __name__ == "__main__":
    sys.exit(main())
