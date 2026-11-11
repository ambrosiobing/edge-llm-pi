#!/usr/bin/env python3
"""Does a grammar's free-text slot admit the project's own instruction, verbatim?

    python3 tools/free_text_slots.py            survey, always exits 0
    python3 tools/free_text_slots.py --check    exits 1 on any slot that admits it

Fifteen of the twenty grammars end a rule with a character class and a repetition range,
which is how this volume lets a model write a sentence under a grammar:

    beam     ::= "\\"" [A-Za-z0-9 ,%-]{8,120} ".\\""        project 6
    note     ::= [A-Za-z0-9 ,.:;!?()/+-]{4,64}              project 19

**Why that shape is a hazard and not a convenience.** The grammar pins the SHAPE of the
field and nothing else, so it cannot distinguish an answer from the question, because the
question has the shape of an answer. Measured twice, on hardware, with the real engine:

    project 6    40 of 40 answers put the prompt's own instruction in `beam`
                 "short sentence saying what is probably in front of the sensor."
                 Friday 30 October 2026, evidence/2026-10-30_1352_...pi4b-eplepi.md

    project 19   1 of 4 notes was the instruction and 3 of 4 were one identical constant,
                 so 0 of 4 said anything about the event
                 Sunday 1 November 2026, evidence/2026-11-01_1650_...pi4b-eplepi.md

In project 19 the downstream field checker reported "0 invented a number" on the same run.
That was not a pass: the check refuses a number absent from the event, and **none of the
four notes contained a digit**, so there was nothing it could refuse. A check whose subject
is numbers cannot fail on prose.

**What this reports, and why it is not merely "the class admits English".** Any class wide
enough for a sentence admits some English, so flagging that would fire everywhere and
discriminate nothing. The question asked here is narrower and has a false case: is a
sentence of the project's OWN prompt a legal instance of its own slot, verbatim? Project 12
answers no, because its class is `[A-Z] [a-z0-9 ,.%-]{8,100}` and admits no capital after
the first character, so any instruction naming "Raspberry Pi" is refused. That single
exception is what makes the check worth running rather than assuming.

**Limits, stated because a reader will otherwise assume more.** Admitting the instruction is
necessary for the echo and not sufficient: it says the failure is available, not that it
happens, and only a run with the real engine says that. This tool does not know what any
project's downstream checker would do with an echo, and it only understands the one rule
shape above, a class with an explicit {min,max}; a grammar that writes its free text some
other way is not examined and is not thereby cleared.

**Reordering the prompt is not the remedy, and that was measured rather than assumed.** On
Sunday 1 November 2026 project 6 was run twice on board A with one variable, the sentence
order of its prompt, both files 448 bytes and the same sentences. With the instruction last,
20 of 20 answers echoed it. With the instruction moved to third of seven, the model echoed
**whatever had become last instead**: two rows listed the decision literals from the rules
section and about twelve read back the event's own input ranges. Zero of twenty answers in
either arm described what was in front of the sensor, and generation doubled, which would
empty that project's speed window on its own. So the model is not following the instruction
and choosing where to put it; it is continuing the text, and a grammar that pins only a
shape cannot refuse a continuation. See project 6's evidence of that date.

**--check currently fails on 14 of 15 slots**, which is the finding rather than a broken
gate, and it is still not in CI for the reason it always was: a gate that fires on almost
everything teaches a reader to ignore it. --check is the survey's verdict and stays the
thing to run when the slots are narrowed or when each project carries a check that rejects
its own instruction. Prompt layout is not that check.

**--ci is the part that CI runs, added Wednesday 11 November 2026, and it is a ratchet
rather than an approval.** It passes on exactly the fourteen slots that admit today and
refuses three changes: a slot that starts admitting and is not on the list, a slot on the
list that stops admitting, and a name on the list with no measured slot behind it. So it
fires on nothing that stands and cannot be satisfied by making the problem bigger. The
second case refuses a genuine improvement on purpose, because a ratchet that does not
notice progress never advances: narrowing a slot is finished by removing it from ADMITTING
in the same change.

**What this does NOT do, stated because the list looks like a waiver.** ADMITTING records
that a slot admits its own instruction, it does not bless it. Fourteen entries is fourteen
open defects, measured twice on hardware, and the remedy is still to narrow the class or to
give each project a check that rejects its own instruction.
"""
import argparse
import pathlib
import re
import sys

ROOT = pathlib.Path(__file__).resolve().parent.parent

# A rule whose body ends in a character class with an explicit repetition range. The prefix
# is kept because two grammars require an initial [A-Z] that the candidate must also satisfy.
SLOT = re.compile(r"^\s*([A-Za-z_]+)\s*::=\s*(.*?)\[([^\]]+)\]\{(\d+),(\d+)\}")


def expand(cls):
    """Expand a GBNF character class body into its set of characters.

    A hyphen is a range only between two characters; a trailing hyphen is a literal, which
    is how four of these grammars admit one.
    """
    out, i = set(), 0
    while i < len(cls):
        if i + 2 < len(cls) and cls[i + 1] == "-" and cls[i + 2] != "]":
            out.update(chr(c) for c in range(ord(cls[i]), ord(cls[i + 2]) + 1))
            i += 3
        else:
            out.add(cls[i])
            i += 1
    return out


def sentences(text):
    """Candidate instruction sentences from a prompt, longest first.

    Longest first on purpose: a short fragment is the easy case and proves little, so the
    example printed is the most substantial sentence that fits.
    """
    parts = re.split(r"(?<=[.;])\s+", text.strip())
    return sorted((p.strip() for p in parts if len(p.strip()) >= 4), key=len, reverse=True)


def project_for(stem):
    """projects/<nn>-... for a grammar named <nn>_<something>.gbnf."""
    hits = sorted(ROOT.glob("projects/%s-*" % stem.split("_")[0]))
    return hits[0] if hits else None


def admissible(prompt_text, prefix, allowed, lo, hi):
    """The first prompt sentence that is a legal instance of the slot, or None."""
    needs_upper_first = "[A-Z]" in prefix
    for s in sentences(prompt_text):
        cand = s
        if needs_upper_first:
            if not cand[:1].isupper():
                continue
            cand = cand[1:]
        # Most of these rules supply the closing full stop as their own literal, so the
        # candidate is compared without one rather than being rejected for carrying it.
        core = cand[:-1] if cand.endswith(".") else cand
        if not lo <= len(core) <= hi:
            continue
        if all(c in allowed for c in core):
            return s
    return None


# The slots that admit a sentence of their own prompt, measured Wednesday 11 November 2026
# and unchanged since the survey of Sunday 1 November 2026. Each pair is (grammar stem, rule
# name). This is a ratchet and NOT an approval: see the module docstring. e12_band is absent
# because it refuses, its class being [A-Z] [a-z0-9 ,.%-]{8,100}, which admits no capital
# after the first character and so refuses any instruction naming "Raspberry Pi".
ADMITTING = frozenset({
    ("e03_colour", "reason"),
    ("e04_thermal", "summary"),
    ("e05_event", "sentence"),
    ("e06_range", "beam"),
    ("e07_pressure", "sentence"),
    ("e08_answer", "answer"),
    ("e11_impact", "line"),
    ("e13_presence", "why"),
    ("e14_inspect", "summary"),
    ("e16_interval", "reason"),
    ("e17_status", "report"),
    ("e18_drift", "summary"),
    ("e19_message", "note"),
    ("e20_next", "reason"),
})


def ratchet(rows, baseline=ADMITTING):
    """Problems that should fail CI, given survey rows and the recorded baseline.

    Pure, so the tests exercise every branch without building a tree of grammars. Keyed on
    (grammar, rule) and not on the grammar alone, because a grammar that grows a second
    free-text slot must register as new rather than overwrite the first.
    """
    problems = []
    measured = set()
    for stem, rule, _lo, _hi, verdict, _example in rows:
        key = (stem, rule)
        measured.add(key)
        if verdict == "ADMITS" and key not in baseline:
            problems.append(
                "NEW: %s's %s slot admits a sentence of its own prompt and is not in "
                "ADMITTING. Narrow the class, or add it and say in the commit why it "
                "stands." % (stem, rule))
        elif verdict == "refuses" and key in baseline:
            problems.append(
                "NARROWED: %s's %s slot no longer admits its own prompt. This is the "
                "good direction; remove it from ADMITTING in the same change."
                % (stem, rule))
        elif verdict not in ("ADMITS", "refuses"):
            problems.append(
                "UNMEASURED: %s's %s slot reports %r, so nothing was compared. A slot "
                "that cannot be measured is not thereby cleared."
                % (stem, rule, verdict))
    for stem, rule in sorted(baseline - measured):
        problems.append(
            "STALE: ADMITTING names %s's %s slot, which is not in the tree. Remove it, or "
            "find out which grammar stopped being surveyed." % (stem, rule))
    return problems


def survey():
    rows = []
    for gram in sorted((ROOT / "grammars").glob("*.gbnf")):
        for line in gram.read_text(encoding="utf-8", errors="replace").splitlines():
            m = SLOT.match(line)
            if not m:
                continue
            rule, prefix, cls, lo, hi = m[1], m[2], m[3], int(m[4]), int(m[5])
            pdir = project_for(gram.stem)
            prompt = pdir / "prompt.txt" if pdir else None
            if prompt is None or not prompt.exists():
                rows.append((gram.stem, rule, lo, hi, "NO PROMPT", ""))
                continue
            hit = admissible(
                prompt.read_text(encoding="utf-8", errors="replace"),
                prefix, expand(cls), lo, hi)
            rows.append((gram.stem, rule, lo, hi, "ADMITS" if hit else "refuses",
                         (hit or "")[:56]))
    return rows


def main():
    ap = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    ap.add_argument("--check", action="store_true",
                    help="exit 1 if any slot admits a sentence of its own prompt")
    ap.add_argument("--ci", action="store_true",
                    help="exit 1 only on a change against the recorded baseline: a new "
                         "admitting slot, a listed slot that now refuses, or a listed "
                         "slot that has left the tree")
    args = ap.parse_args()

    rows = survey()
    print("%-18s %-10s %4s %4s  %-9s %s"
          % ("grammar", "rule", "min", "max", "verdict", "first admissible prompt sentence"))
    print("-" * 110)
    for stem, rule, lo, hi, verdict, example in rows:
        print("%-18s %-10s %4d %4d  %-9s %s" % (stem, rule, lo, hi, verdict, example))

    admits = [r for r in rows if r[4] == "ADMITS"]
    print("\nfree_text_slots: %d slots, %d admit a sentence of their own prompt verbatim"
          % (len(rows), len(admits)))

    if args.ci:
        problems = ratchet(rows)
        for item in problems:
            print(item)
        print("free_text_slots --ci: %d slots measured, %d against the baseline of %d"
              % (len(rows), len(problems), len(ADMITTING)))
        return 1 if problems else 0

    if not args.check:
        return 0
    if admits:
        print("free_text_slots: %d slots admit their own instruction" % len(admits))
        return 1
    return 0


if __name__ == "__main__":
    sys.exit(main())
