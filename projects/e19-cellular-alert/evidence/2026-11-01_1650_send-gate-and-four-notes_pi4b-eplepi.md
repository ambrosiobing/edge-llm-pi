# e19: the send gate holds, and the four notes carry no information, Sunday 1 November 2026, 16:50

    board      Raspberry Pi 4 Model B Rev 1.4, 8 GB, hostname eplepi, revision d03114
    model      qwen2.5-0.5b-instruct-q4_k_m.gguf, 491400032 bytes
    engine     llama.cpp v0.6.0, the pinned tag, real engine build at build/
    source     replay from projects/e19-cellular-alert/replay/alerts_50.csv, 50 events
    settings   e19's own grammar and prompt, --state /tmp/e19.state
    register   0x0 before and 0x0 after; 39.9 C before, 56.4 C after
    tree       60b299b, the board fast-forwarded to it from 3c4aa18 the same afternoon
    absent     no accelerometer, no modem, no SIM; the events are the replayed table

**The board was chosen rather than the chapter's Pi 3B+, and that is a departure.** e19's
manifest names the Pi 3B+. The three quantities below are properties of the model's text and
of the gate's logic, neither of which is a property of the board, so the faster machine with
8 GB costs nothing and the departure is recorded here rather than left to be noticed. A
latency claim would not be transferable this way and none is made.

## WHAT PASSES, EVENT BY EVENT

The send gate is the reason this chapter exists, and it is correct.

    t_s    rule       armed    why
    5.0    TAP        yes      first arming, nothing had been armed before
    7.0    TAP        no       2 s after the arming at 5.0
    12.0   TAP        no       7 s after the arming at 5.0, not 5 s after the event at 7.0
    20.0   FREEFALL   yes      15 s after the arming at 5.0
    21.0   FREEFALL   no       1 s after the arming at 20.0
    31.0   TAP        yes      11 s after the arming at 20.0
    44.0   TAP        yes      13 s after the arming at 31.0

    4 armed, 3 blocked by the minimum interval, which is what the manifest predicted

**The clock counts armings and not successful sends**, exactly as `rules_e19.c` says it does in
its own comment, and the row at 12.0 s is the proof: it is measured from the arming at 5.0
rather than from the refused event at 7.0. A refused message consumes the interval.

## THE FIGURE THE ACCEPTANCE ASKS FOR CANNOT FAIL AS MEASURED

    e19: 50 events (NONE 37, TAP 5, TILT 6, FREEFALL 2), 4 armed, 3 blocked by the
    minimum interval, 4 composed, 0 model errors, 4 passed the checker, 0 refused,
    0 invented a number, 0 sent, 4 left queued, 0 dropped, modem not fitted

`0 invented a number` reads as a pass. **It is not one, and the reason is in the four notes:**

    "Check the device settings for the maximum acceleration limit."   3 of 4, identical
    "Check the message type, peak value, and time."                   1 of 4

**NONE OF THE FOUR NOTES CONTAINS A DIGIT.** The field checker in `src/compose_check.c`
refuses a message carrying a number that is in no field of the event. Where a note contains no
number, there is nothing for it to refuse. So the published count of invented numbers was
taken over four notes that together offered the check nothing to examine, and a count of zero
is what this run was structurally obliged to produce.

The three skeleton fields were copied correctly and that is real, but it is also the part the
grammar already pins: `peak ::= dig "." dig dig` admits only that shape and the model filled
it from the event. **The checker's power is over the note, and the note had no numbers.**

### And one note is the instruction read back

    prompt.txt   "...and then one short note of at most sixty characters saying what a
                  reader should check."
    the model    "Check the message type, peak value, and time."

That note tells the reader to check the three fields printed in the same message, in the
prompt's own vocabulary. **It is the instruction, not a note.** The other three are one
identical constant about device settings, which is not about the event either: it is the same
sentence whether the peak was 2.33 g or 3.36 g, and it was emitted for TAP three times.

**So zero of four notes say anything specific to the event**, and the grammar cannot see it:

    note ::= [A-Za-z0-9 ,.:;!?()/+-]{4,64}

Four to sixty-four characters from that class. The instruction is a legal instance and so is a
constant. **This is the same defect e06 recorded on Friday 30 October 2026**, where 40 of 40
answers put the prompt's own instruction in the `beam` field under a grammar that constrained
only a shape. Two projects, two slots for free text, the same outcome, and in both cases the
only check in place is structural.

## THE LEDGER SAYS `sent` WHERE NOTHING WAS SENT

Four rows carry `verdict` of `"sent"`. In the same rows the `sent` column reads `0`, the `mr`
column is empty, and `queued` counts up 1, 2, 3, 4. The summary line says `0 sent, 4 left
queued, modem not fitted`.

**The chapter is careful that delivery reads "not measured" with its reason wherever it
appears, and the ledger's own verdict column contradicts that.** A reader scanning the ledger
sees four messages sent. The columns beside it say otherwise and the verdict is the one that
reads like prose. It should say what happened: passed the checker, queued, modem not fitted.

## A PROPERTY OF THE GATE WORTH PUBLISHING, WHICH THE CHAPTER DOES NOT CARRY

    5.0   TAP   peak 2.33 g   ARMED and composed
    7.0   TAP   peak 2.50 g   blocked
    12.0  TAP   peak 3.05 g   blocked

**The gate sent the smallest of the three and suppressed both larger ones.** The interval is
purely temporal and has no magnitude escape, so the first knock inside a window wins over a
harder one arriving seconds later. For a safety alert that is a real design consequence and it
follows directly from the rule the chapter is proud of: the interval exists to prevent an
alert storm, and the price is that it cannot be preempted by severity. Worth stating as a
limit rather than discovered later by a reader.

## AN OVERSTATEMENT OF MINE, CORRECTED HERE

Before the run I counted the clause "zero sends on any event the rule did not arm" as having
**46 negative cases**. It has 3.

The ledger holds seven rows, one per TAP and FREEFALL event. The 37 NONE and 6 TILT events
produce no row at all, because `rule_e19_sends` returns 0 for index 0 and index 2 before
anything is written. So the clause is witnessed by a recorded refusal for 3 events and by the
**absence of a row** for 43. The code guarantees those 43 and the guarantee is one line, but
absence of a row is also what "never considered" looks like, and a clause evidenced by absence
is weaker than one evidenced by a verdict. See the volume's own rule that a claim of absence
is unfalsifiable.

## What is established and what is not

    established   the send gate arms 4 of 7 and blocks 3 on the minimum interval, and the
                  interval is measured from the last ARMING rather than the last event
    established   the register stayed 0x0 across the run, 39.9 C to 56.4 C, 4 model calls
    established   the three skeleton fields were copied from the event without rounding
    established   0 of 4 notes carry information specific to the event: 3 are one constant
                  and 1 is the prompt's instruction read back
    established   the ledger's verdict column reads "sent" on 4 rows where sent = 0
    established   the gate suppressed 3.05 g and 2.50 g while sending 2.33 g
    NOT a pass    "0 invented a number", because the four notes contain no digits and the
                  checker had nothing it could refuse
    NOT shown     e19's oracle, which registers only in a stub build; build/ is a real
                  engine build and the manifest's test line does not say so
    NOT measured  delivery, which needs a SIM and a second telephone, as the chapter says
