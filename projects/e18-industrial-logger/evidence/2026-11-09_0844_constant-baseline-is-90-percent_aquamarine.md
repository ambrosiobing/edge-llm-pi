# e18: the replay's constant baseline is 90.3 percent, Monday 9 November 2026, 08:44

    host       aquamarine, the authoring laptop. NO BOARD, no engine, no model, no HAT
    subject    projects/e18-industrial-logger/replay/registers_600.csv, 600 frames
    rule       rules.rs: DRIFT when the span of register 0 over the last 30 frames
               exceeds 100 counts, a tenth of its declared range of 0 to 1000
    method     arithmetic over the replay file. The rule's condition is a span over a
               window, so what the rule answers on every frame is computable without
               running anything
    tree       33f3189

**THIS CLOSES NO CLAUSE AND e18 HAS STILL NEVER RUN ON HARDWARE.** No board was powered, no
HAT was seated, no model was loaded. What this establishes is the number the chapter's second
clause has to be read against, computed before a session is spent producing a figure that
cannot be interpreted without it.

## WHY THIS WAS COMPUTED FIRST

Two projects in this volume published an agreement figure that a constant equals or beats, and
both were found after the measurement rather than before:

    e20   the model scored 30.0 percent where a constant scored 100
    e06   the model answered one word on every event and scored 54.0 percent, which is
          exactly what a constant of that word scores on the same rows. The same output
          scored 0.0 percent on a different slice of the same file, because the first
          twenty rows are all one class.

`tools/agreement.py` prints the baseline beside the rate since Friday 6 November 2026, so a
future e18 run will report it automatically. **But the baseline is a property of the replay
file and not of the run**, so it can be known now, and knowing it now is what decides whether
the clause is worth a sitting.

## THE RULE'S OWN ANSWERS OVER THE 600 FRAMES

    verdict     frames   share
    NOMINAL        542   90.3 percent
    DRIFT           58    9.7 percent

    a constant NOMINAL predictor scores 542 of 600, 90.3 percent
    register 0 ranges from 410 to 601 counts across the file

## THE SLICE IS NOT DEGENERATE, WHICH IS THE GOOD NEWS

e06's problem was a one-class slice: the first twenty rows of its replay are all CLEAR, so a
constant won outright and every figure taken there measured the class balance. **This stream
is not that.** Both verdicts appear and the rule crosses between them four times:

    frame 180   NOMINAL -> DRIFT
    frame 209   DRIFT -> NOMINAL
    frame 400   NOMINAL -> DRIFT
    frame 429   DRIFT -> NOMINAL

Two excursions, each producing **29 consecutive DRIFT frames**, which is the 30-frame window
sliding across an injected step. So the generator put two events in a quiet stream and the
rule's window turns each into a 29-frame episode. A model has four transitions to find and 58
frames to get right, and a model that never says DRIFT still scores 90.3 percent.

## WHAT THIS MEANS FOR THE CLAUSE, STATED BEFORE THE RUN

The acceptance reads: "a 1000-frame RS485 half-duplex echo at 115200 baud with zero errors;
**the agreement table over the 600-frame replayed register stream**; the ABI's cost per call
against an in-process call."

**The clause asks for a table and does not say what would count as success.** With a 90.3
percent baseline, three outcomes are worth distinguishing in advance:

    above 90.3 percent      the model adds something, and by how much is the finding
    at about 90.3 percent    the model is a constant NOMINAL predictor, which is what e06's
                            model turned out to be, and the figure is the class balance
    below 90.3 percent      the model is worse than saying nothing, which e20 already
                            published at 30.0 against a constant's 100

**The second is the outcome to expect** on the evidence so far: a 0.5B model on this volume's
tasks has answered with one constant literal in two projects out of two measured. If it does,
that is a publishable finding and not a failure, and it should be published with the baseline
beside it rather than as a bare percentage.

## WHAT THIS DOES NOT ESTABLISH

    established   the rule's verdicts over the replay: 542 NOMINAL, 58 DRIFT, 4 transitions
    established   a constant NOMINAL predictor scores 90.3 percent on this stream
    established   the slice is not one-class, so the clause is measurable
    NOT shown     anything whatever about the model, which has not been run
    NOT shown     e18's first clause, the 1000-frame echo, which needs the WS-28164 seated
                  on the Pi 4B with card B, the sc16is752-spi1 overlay, the console released
                  and two leads across the HAT's RS485 screw terminals
    NOT shown     e18's third clause, the ABI cost, which needs the binary built
    NOT RUNNABLE  on board A: e18 is Rust and that board has no Rust toolchain. The build
                  clone is skyhorizon's WSL, cargo 1.99.0, against this manifest's floor
    NOT CHECKED   whether skyhorizon holds qwen2.5-0.5b-instruct-q4_k_m.gguf, which clause
                  two needs; board A has it at 491400032 bytes
