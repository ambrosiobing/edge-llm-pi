# e19: narrowing the free-text class refuses the instruction and kills the answers that
# say something, Thursday 12 November 2026, 13:52

    host       aquamarine, the authoring laptop. NO BOARD, no engine, no model
    subject    the 15 free-text slots of grammars/*.gbnf, and every measured answer in
               projects/*/evidence/*.csv
    method     strip A-Z from each slot's character class, as project 12's class already
               does, then ask twice: does the project's own instruction still fit, and do
               the answers a real engine actually produced still fit
    tool       the slot survey of tools/free_text_slots.py, reused
    tree       ffdcd59

**THE REMEDY IS REFUTED AND NOT POSTPONED.** `tools/free_text_slots.py` names two conditions
for wiring its check into CI: narrow the slots, or give each project a check that rejects its
own instruction. This record closes the first of those as the wrong move, by measurement.

## STRIPPING A-Z WOULD REFUSE THE INSTRUCTION IN TWELVE OF FOURTEEN SLOTS

Project 12 is the one slot that already refuses its own prompt, and the reason is its class:
`[A-Z] [a-z0-9 ,.%-]{8,100}` admits no capital after the first character, so any sentence
naming "Raspberry Pi" or a decision literal is refused. Applying that one change, removing
A-Z from the class body, to every other slot:

    refuses the instruction after the change    12 slots
    still admits it                              2 slots, e06_range and e19_message
    already refused                              1 slot, e12_band

e06 and e19 survive because their instructions happen to carry no capital past the first
character. So on the instruction side the change looks like a near-complete remedy.

## AND IT KILLS THE ANSWERS THAT SAY SOMETHING, WHICH IS THE WHOLE POINT

The same change against every answer a real engine produced in this repository:

    ledger                  values   fit today   fit after   first answer it would refuse
    e03_run.csv                200         200         187   "The level was above 60 and
                                                             the slope was above 10 per
                                                             second."
    e04_replay.csv              44          44           0   "46.3 C, 0x0, 1500 MHz,
                                                             none, previous temper..."
    e06_qwen05b.csv             20          20           0   "The two newest ranges are
                                                             both below 300 mm."
    e06_armA.csv                20          20          20   nothing: all twenty are the
                                                             lower-case instruction echo
    e06_descent_sampled.csv     47          47          47   nothing, same reason

**The change is aimed backwards in all three projects that have measured answers.**

In project 3, 187 of the 200 answers are the template `level #, slope # per second` and they
survive, because a template of lower-case words and numbers needs no capital. The 13 that
would be refused are the only ones that read as an explanation rather than a restatement.

In project 6, the twenty rows of arm A survive untouched, and every one of them is the
instruction echoed verbatim; it survives because the echoed clause is lower case. The twenty
answers of the 0.5B run are all refused, and those are the ones that state a fact about the
ranges.

In project 4 every answer is refused, because the field it must report carries SoC, C, ARM and
MHz. A class with no capitals cannot express the units the prompt requires.

## WHY THE DIRECTION IS WRONG, STATED SO IT IS NOT TRIED AGAIN

A capital letter is not a marker of an instruction. In this volume the instructions are
lower-case prose in the middle of a prompt, and the capitals live in what a correct answer
needs: the start of a sentence, an acronym, a unit, an operator name, a decision literal
quoted back. **So the class that refuses the instruction is the class that also refuses the
answer**, and project 12 is not a model to copy but a project whose answer happens to need no
capital.

More generally, a character class pins the shape of a field and an instruction has the shape
of an answer, which this volume recorded on Sunday 1 November 2026 and measured again here
from the other side. The remaining remedy is the second one the tool names, a check per
project that rejects that project's own instruction, and
`projects/e03-led-explainer/evidence/2026-11-12_1112_shape-count-not-distinct-count_aquamarine.md`
reports what that catches and what it misses: project 6's verbatim echo yes, project 19's
paraphrase no.

## AND THE STUB'S FULL STOP IS NOT A GRAMMAR VIOLATION

Checking the same classes against the stub ledgers reported 0 of 100 answers valid, the stub
writing `"."` where the rule asks for 8 to 120 characters plus a full stop. That is documented
behaviour, not a defect: `common/c/src/edgellm_stub.c` states in its own header that a
character class and every repetition mark append nothing, and that a sentence grammar made of
character classes yields an empty answer which the project records in its oracle file. The
full stop is the rule's trailing literal. Checked before reporting.

## WHAT THIS DOES NOT ESTABLISH

    established   stripping A-Z refuses the instruction in 12 of the 14 admitting slots
    established   it also refuses every measured answer in e04 and in e06's 0.5B run, and
                  13 of 200 in e03, which are the 13 that explain rather than restate
    established   it refuses none of the 20 verbatim echoes of e06 arm A
    established   so the narrowing condition in free_text_slots.py's docstring is the
                  wrong remedy, and the docstring now says so
    established   the stub's full stop is its documented output, not a violation
    NOT shown     whether some other class, not built on capitals, could separate the two.
                  Nothing here rules that out; what is ruled out is the capital rule
    NOT shown     anything about a model under a changed grammar, which needs a run
