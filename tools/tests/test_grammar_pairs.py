"""The pair check of tools/grammar_pairs.py, including the cases that must refuse.

Every grammar in this repository exists twice, once in the project folder where the oracle
test reads it and once under grammars/ where the chapter cites it. Nothing compared them
until this check existed, so a check that has never refused anything would be no better.
Four of the tests below build a small tree on purpose wrong.
"""
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.abspath(__file__))))

import grammar_pairs  # noqa: E402

GRAMMAR = 'root ::= "NORMAL" | "WATCH" | "ALERT"\n'


def build(tmp, named="grammars/e99_thing.gbnf", canonical=GRAMMAR, copy=GRAMMAR,
          write_canonical=True, write_copy=True):
    """A two-file tree shaped like the real one, with each half controllable."""
    # write_bytes, not write_text. On Windows the text writer translates every "\n" into
    # "\r\n", so a case written to carry CRLF deliberately arrives as "\r\r\n" and the test
    # fails for a reason that has nothing to do with the code under test. That happened on
    # the first run of this file, Friday 16 October 2026, and the line-ending case is
    # precisely the one where it matters.
    proj = tmp / "projects" / "e99-thing"
    proj.mkdir(parents=True)
    (tmp / "grammars").mkdir(exist_ok=True)
    (proj / "project.toml").write_bytes(
        ('[project]\nid = "e99"\ngrammar = "%s"\n' % named).encode("ascii"))
    if write_canonical and named.endswith(".gbnf"):
        target = tmp / named
        target.parent.mkdir(parents=True, exist_ok=True)
        target.write_bytes(canonical.encode("ascii"))
    if write_copy:
        (proj / "grammar.gbnf").write_bytes(copy.encode("ascii"))
    return tmp


def test_the_repository_itself_is_consistent():
    """Nineteen projects decide something; e01 is the instrument and has no grammar."""
    seen, problems = grammar_pairs.check()
    assert problems == []
    assert seen == 19


def test_identical_copies_pass(tmp_path):
    seen, problems = grammar_pairs.check(str(build(tmp_path)))
    assert seen == 1
    assert problems == []


def test_drift_is_refused(tmp_path):
    """The case the check exists for: someone edits one copy and not the other."""
    root = build(tmp_path, copy=GRAMMAR + 'extra ::= "DRIFT"\n')
    _, problems = grammar_pairs.check(str(root))
    assert len(problems) == 1
    assert "drifted apart" in problems[0]


def test_a_line_ending_difference_alone_is_not_drift(tmp_path):
    """This tree is edited on Windows and built on Linux; CRLF is not an edit."""
    root = build(tmp_path, copy=GRAMMAR.replace("\n", "\r\n"))
    _, problems = grammar_pairs.check(str(root))
    assert problems == []


def test_a_missing_folder_copy_is_refused(tmp_path):
    """The oracle test asks for this file by name, so its absence is not a pass."""
    root = build(tmp_path, write_copy=False)
    _, problems = grammar_pairs.check(str(root))
    assert len(problems) == 1
    assert "no grammar.gbnf" in problems[0]


def test_a_manifest_naming_a_missing_grammar_is_refused(tmp_path):
    root = build(tmp_path, write_canonical=False)
    _, problems = grammar_pairs.check(str(root))
    assert len(problems) == 1
    assert "does not exist" in problems[0]


def test_a_prose_grammar_field_is_skipped_not_misread(tmp_path):
    """e01 writes a sentence there; a sentence is not a path and is not a problem."""
    root = build(tmp_path, named="none: this project decides nothing, it is the instrument",
                 write_canonical=False)
    seen, problems = grammar_pairs.check(str(root))
    assert seen == 0
    assert problems == []
