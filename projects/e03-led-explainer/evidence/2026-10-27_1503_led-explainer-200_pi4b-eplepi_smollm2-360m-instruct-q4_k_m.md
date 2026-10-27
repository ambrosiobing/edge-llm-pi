# e03, the LED status explainer, measured Tuesday 27 October 2026, 15:03

    board      Raspberry Pi 4 Model B Rev 1.4, 8 GB, Cortex-A72, four threads
    card       eplepi, Raspberry Pi OS, Debian 13 trixie
    HAT        none, bare header
    parts      three Joy-it LK-LED10 modules, green on GPIO17, yellow on GPIO27,
               red on GPIO22, grounds on header pins 14, 20 and 25, V pins
               unconnected on all three
    model      SmolLM2-360M-Instruct-Q4_K_M.gguf, 270590880 bytes,
               sha256 2fa3f013dcdd7b99f9b237717fa0b12d75bbb89984cc1274be1471a465bac9c2
    engine     llama.cpp v0.6.0, the pinned tag
    source     replay/state_200.csv, 200 events
    log        evidence/e03_run.csv, 201 lines, 31569 bytes
    photo      evidence/2026-10-27_1503_wiring_pi4b-eplepi.jpg

Two hundred replayed events, three LED modules driven live, the model asked for the colour
the rule chose and one line naming why. Three of four clauses pass, two of them at 100.0
percent, and the one that fails is the board's power supply rather than anything in the
project.

## What was measured

    e03: 200 events, 200 model answers, 0 model errors, 0 actuation errors,
         mean prompt 1320.7 ms, mean generate 3448.3 ms, rss 352896 kB,
         engine llama.cpp v0.6.0, source replay

    N 200, scored 200, empty 0, unparsed 0
    colour agreement   200 of 200, 100.0 percent
    reason names field 200 of 200, 100.0 percent

    rule \ model      GREEN   YELLOW      RED
    GREEN               135        0        0
    YELLOW                0       38        0
    RED                   0        0       27

The confusion matrix is diagonal with no off-diagonal cell at all, across all three classes,
and the class counts sum to the 200 events: 135 GREEN, 38 YELLOW, 27 RED.

## The clauses

    colour agreement over 200 events            PASSES   200 of 200, 100.0 percent
    reason names the field the rule triggered   PASSES   200 of 200, 100.0 percent
    zero rows with no parseable object          PASSES   0 of 200 unparsed, 0 empty
    get_throttled 0x0 before and after          FAILS    0x0 before, 0xd0000 after
    LED current from the PPK2 figures           already measured Saturday 3 October 2026

The failing clause is read as the board and not the project: see the power section below.

## What this says against e04, which is the point of measuring both

e04 was measured on the same board with the same model, the same engine tag and the same
`--n-predict 64`, on Monday 26 October 2026. It scored 42.2 percent, never once emitted its
third class across fifty-one events that called for it, exhausted the token budget on 88.3
percent of rows and left 75.6 percent of its objects unterminated.

    project   agreement        classes reached   objects unclosed   rows at the ceiling
    e03       200/200  100.0%  3 of 3            0 of 200           none
    e04        76/180   42.2%  2 of 3            136 of 180         159 of 180

Same model, same budget, same grammar machinery, same board, same week. **So the volume's
through-line question does not have a model-shaped answer, it has a task-shaped one.** Asked
to name which field a two-line threshold tripped, a 360M model is right two hundred times
out of two hundred. Asked for up to 160 characters of free prose about a hex throttle word,
the same model misses every throttling event and does not close three quarters of its JSON.

The difference is visible in the answers themselves. e03's reasons are terse and factual:

    { "colour": "YELLOW", "reason": "level 78.2, slope 1.9 per second" }

while a few rows reach for prose and still close inside the budget:

    { "colour": "YELLOW", "reason": "The level was above 60, and the slope was above 10 per second." }

e04's degenerated into repeating its own enum literals until the budget cut the string.

Neither number alone would have shown this. A single project measuring 42 percent reads as a
weak model; a single project measuring 100 percent reads as an easy task. The pair is the
result.

## Power, which is now the binding constraint on this bench

The register read `0x0` immediately before the run and `0xd0000` immediately after: bit 16
under-voltage occurred, bit 18 throttling occurred, bit 19 the soft temperature limit
occurred. The sticky bits do not clear until boot, and the run began on a clean boot with a
confirmed `0x0`, so the run itself did it.

**This is the second consecutive run on this board to dirty the register**, after e04 went
`0x0` to `0xf0000` on Monday 26 October 2026. Two for two, on a board that also rebooted
under a `-j4` build on Friday 16 October 2026 and left two zero-byte object files behind.

What it does and does not invalidate:

- **The agreement figures stand.** They compare the rule against the model over a replayed
  stream; throttling changes how long a decision takes, not which literal comes back.
- **The latency figures carry the label.** Mean prompt 1320.7 ms and mean generate 3448.3 ms
  are from a run that undervolted and throttled partway, so they are an upper bound on this
  board under this supply and not a clean figure.
- **The clause fails and is not argued away.** The acceptance says `0x0` before and after.

A power supply is not on the volume's "what would have to be bought" list, which currently
ranks a multimeter first. On the evidence of two runs it belongs above the multimeter: every
timing figure the remaining projects produce on this board is suspect until it is replaced.

## Deviations from what the chapter prints, all four stated

1. **The board is the Pi 4 Model B Rev 1.4, not the Pi 3 Model B the chapter allocates.**
   e03's acceptance is board-independent, being two fractions over a replayed stream plus a
   current figure already measured, so the clauses are unaffected. The Pi 3 was chosen in
   the chapter for HAT allocation reasons, and the Pi 4B was used because it already carried
   a built engine and the model, where the Pi 3 would have needed over an hour of building
   first. The latency and resident-memory figures belong to a Cortex-A72 with 8 GB and are
   not the 1 GB Cortex-A53 rung the chapter describes.
2. **Run as `bing` from a root login rather than through the systemd unit**, so the unit's
   own group and restart behaviour was not exercised.
3. **The build was configured and compiled as root in `bing`'s tree**, which left root-owned
   objects that were corrected with `chown` before the run. The configure also reported
   `ggml commit: unknown` from git refusing a repository it did not own; the engine string
   the log records comes from the pinned literal in `cmake/llama_tag.cmake` and not from
   git, so `llama.cpp v0.6.0` in the log is correct.
4. **The leads were wired in the wrong order and it was caught before the run, not after.**
   Driving each line by hand gave yellow on GPIO17, red on GPIO27 and nothing on GPIO22,
   against a chapter that prints green, yellow, red in that order. After rewiring, the three
   lines gave green, yellow and red correctly. Had this not been checked, the run would not
   have failed loudly; it would have returned a wrong colour agreement, and a wrong
   agreement is indistinguishable from a wrong model.

## Numbers worth keeping beside the figures

Resident memory 352896 kB, which is 344.6 MiB, against a model file of 258.1 MiB and a CPU
compute buffer the engine reported as 105.8945 MiB, matching its own expectation exactly. So
the GGUF is memory mapped and not wholly resident, which on an 8 GB board is ordinary lazy
paging and is the figure to compare against when this project is repeated on the 1 GB rung,
where it is the whole question.

The state was restored rather than decoded cold, as `state_read_data` reports, so the prompt
figure is a restore and not a cold pass.

Mean total decision 4769 ms, so two hundred events took about sixteen minutes.

`build/tests/oracle_check` reported 200 rows and 200 differences, which is expected and not a
finding: the fixture holds the stub's empty reason, so a real model differs on every row by
construction. The published figures are the two fractions above.

## The photograph

`2026-10-27_1503_wiring_pi4b-eplepi.jpg`, taken **during this run**: its camera timestamp
read 14:55:04 and the log was last written at 15:03, against a run that took about sixteen
minutes, so the shutter fell roughly eight minutes in.

What it documents, and it is the thing that went wrong earlier in the sitting: the three
modules left to right are **green, yellow, red**, matching the order the chapter prints,
after the rewire. Two leads on each module and nothing on the `U` pin, which is the condition
the PPK2 figures of Saturday 3 October 2026 were measured under. The leads are yellow and
black on green, yellow and green on yellow, white and orange on red; which of each pair is
the signal lead is not recorded, so `wiring.md`'s wire-colour column still reads
`not recorded` and should be filled from the bench rather than from this image.

**What the photograph does not establish.** All three domes look lit in it, and e03's loop
drives one line at a time. A diffused coloured dome under a bright light looks the same as an
emitting one, so this image cannot distinguish the two, and nothing here should be read as
three lines being high at once. If that matters later it is a question for the log's rule
column, not for the photograph.

The file carries no metadata. It was stripped of its EXIF block and reduced from 4080x2296
at 2.1 MB to 1600x900 at 200 KB before being committed, because it is the first image in this
repository and the repository is public: the original carried the camera's manufacturer,
model and firmware string. It carried no GPS tags, which was checked before anything was
committed rather than assumed.

## How the log reached this repository

`scp` from the board to the authoring laptop **after the run had exited**, then verified
against the board's own count before anything was written from it: 201 lines and 31569 bytes
on both sides, no carriage returns. That check exists because e02's register trace was once
committed as a 1329-line prefix of its own 1501-line run, copied while the sampler was still
writing. A log is complete only when its writer has exited.
