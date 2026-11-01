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

**--check currently fails on 14 of 15 slots**, which is the finding rather than a broken
gate. It is not wired into CI for that reason: a gate that fires on almost everything
teaches a reader to ignore it. Wire it in when the slots have been narrowed or when each
project carries a check that rejects its own instruction.
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
    if not args.check:
        return 0
    if admits:
        print("free_text_slots: %d slots admit their own instruction" % len(admits))
        return 1
    return 0


if __name__ == "__main__":
    sys.exit(main())
