# e03 on the Pi 3B+, the 1 GB rung answered, Tuesday 27 October 2026, 17:53

    board      Raspberry Pi 3 Model B Plus Rev 1.3, revision code a020d3,
               905 MB, Cortex-A53 at 1.4 GHz, four threads
    card       Raspberry Pi OS, Debian 13 trixie, user bing, 10.42.0.69
    hostname   rpi3BV1p2, WHICH NAMES A REVISION THIS BOARD IS NOT; see below
    HAT        none, bare header
    parts      none. The three LED modules stayed on the Pi 4B, so this run
               used no --leds and exercised no GPIO line
    model      SmolLM2-360M-Instruct-Q4_K_M.gguf, 270590880 bytes,
               sha256 2fa3f013dcdd7b99f9b237717fa0b12d75bbb89984cc1274be1471a465bac9c2
    engine     llama.cpp v0.6.0, the pinned tag, built on this board
    source     replay/state_200.csv, 200 events
    log        evidence/e03_run_rpi3bplus.csv, 201 lines, 31569 bytes

The chapter's own rung. e03 is written for a 1 GB Pi 3 and had only ever been measured on an
8 GB Pi 4B, so every figure it published belonged to a board the chapter does not describe.
This run puts it on the class of board it was written for, and answers two questions the Pi 4B
could not: whether a 360M model fits a 1 GB board without paging, and whether this project's
perfect agreement was a property of the hardware.

## What was measured

    e03: 200 events, 200 model answers, 0 model errors, 0 actuation errors,
         mean prompt 3410.1 ms, mean generate 6950.3 ms, rss 349720 kB,
         engine llama.cpp v0.6.0, source replay

    N 200, scored 200, empty 0, unparsed 0
    colour agreement   200 of 200, 100.0 percent
    reason names field 200 of 200, 100.0 percent

    rule \ model      GREEN   YELLOW      RED
    GREEN               135        0        0
    YELLOW                0       38        0
    RED                   0        0       27

    get_throttled   0x0 before, 0x80000 after
    temperature     67.1 C during, 45.6 C after cooling
    duration        about 34.5 minutes, 10.4 s per event

## The clauses

    colour agreement over 200 events            PASSES   200 of 200, 100.0 percent
    reason names the field the rule triggered   PASSES   200 of 200, 100.0 percent
    zero rows with no parseable object          PASSES   0 of 200 unparsed, 0 empty
    get_throttled 0x0 before and after          FAILS    0x0 before, 0x80000 after
    LED current from the PPK2 figures           not exercised; no LEDs on this board

The failing clause is **thermal and nothing else**, which is the finding rather than the
failure: see the two sections below.

## The 1 GB rung holds, and the rule predicted it

    MemAvailable before the run    741548 kB = 724.2 MiB
    the model file                 258.1 MiB = 35.6 percent of available
    the 60 percent threshold       434.5 MiB
    resident memory during         349720 kB = 341.5 MiB = 47.2 percent of available
    swap used                      0 of 904 MB

**Zero swap, start to finish.** The ladder's rule of thumb, that the model file must sit under
60 percent of `MemAvailable` or the figures are SD-card paging rather than inference, was
written down before this run and held: the file needed 434.5 MiB of room and wanted 258.1.

So the 10.4 seconds per event is the Cortex-A53, not the card. That distinction is the whole
reason the rule exists, and this is the first run in the volume where it was at risk.

Resident memory is also effectively constant across every configuration measured today:
341.5 MiB here, 343.0 on the Pi 4B at two threads, 344.6 at four. **Neither thread count nor
board moves the footprint**, which is what makes the 60 percent rule portable between rungs.

## The agreement is not a property of the hardware

Five runs of this project now exist, across two microarchitectures, two thread counts and an
eightfold difference in memory. The `model` column of all five logs is **byte-identical on 200
of 200 rows**.

    Pi 4B  Cortex-A72  4 threads  8 GB     white ISY    15:03
    Pi 4B  Cortex-A72  2 threads  8 GB     white ISY    21:40
    Pi 4B  Cortex-A72  4 threads  8 GB     white ISY    18:25
    Pi 4B  Cortex-A72  4 threads  8 GB     black 65 W   19:05
    Pi 3B+ Cortex-A53  4 threads  905 MB                17:53

**These are not five measurements that happened to agree. They are one set of two hundred
answers, produced five times.** The agreement fractions and the confusion matrix could not
have differed between them, because the bytes being scored are the same bytes. The engine is
bit-exact reproducible here across thread count and across CPU, which is not a given, since
thread count changes the order of floating-point reductions.

What that buys is the strength of the volume's through-line claim. e04 scored 42.2 percent on
the same model, the same budget and the same engine; e03 scores 100.0 on five configurations.
**The variable was never the hardware and never the model. It is the question asked.**

## The supply and the heat are two different faults, and this board separates them

`0x80000` is bit 19 alone, the soft temperature limit. **Bit 16, under-voltage, was never set
at any point in 34.5 minutes** at four threads, which is the longest run on either board
today, longer than the 16-minute Pi 4B run that set three bits.

    board   threads  duration     register after   fault
    Pi 4B         2  21.6 min     0x0              none
    Pi 4B         4  15.9 min     0xd0000          supply first, then heat
    Pi 3B+        4  34.5 min     0x80000          heat only

So two things that had been read as one bench-wide problem are two unrelated problems with
two different remedies, and the remedy for one does nothing for the other. **Duration is also
ruled out conclusively**: the longest run of the day is the one with the cleanest supply
reading.

The soft limits differ by board and the published figures should say so rather than carry one
number: this board engaged bit 3 by 67.1 C, while the Pi 4B's trace shows bit 3 engaging
around 80 C.

## Speed, against the Pi 4B at the same four threads

    board        prompt      generate    total        per token
    Pi 4B A72    1320.7 ms   3448.3 ms    4769.0 ms   121.9 ms
    Pi 3B+ A53   3410.1 ms   6950.3 ms   10360.4 ms   245.6 ms
    ratio        2.58        2.02        2.17         2.02

All 200 rows generated slower on the A53, without a single exception.

**The A72's lead is larger on the prompt than on the generation, 2.58 against 2.02**, and that
is the same split the thread experiment found on one board earlier the same day, where two
extra threads bought 1.94 on the prompt and 1.14 on the generation. A prompt is a batched
matmul over many tokens at once and rewards both cores and core quality; a generation emits
one token at a time and waits on memory, so it rewards them less. **A third and fourth
route to a conclusion this volume already held.**
[Project 1](../../chapters/01-bring-up-and-the-honest-tokens-per-second.md) measured it first
on Friday 16 October 2026 with `llama-bench`, where prompt processing scaled 3.72 times over
four cores while generation rose to three threads and fell back at four. e03 now reaches the
same split twice more, once by changing the thread count and once by changing the CPU, on a
real project loop rather than a synthetic benchmark. None of these is a discovery; together
they are an unusually well-supported claim.

## The hostname names a revision this board is not

`/proc/cpuinfo` reports `Revision: a020d3`, which is a **Raspberry Pi 3 Model B Plus Rev 1.3**,
and `/proc/device-tree/model` agrees. The card's hostname is `rpi3BV1p2`, which reads as a
Pi 3B v1.2 and is wrong on both the model and the revision.

The revision code is authoritative and the hostname is a label somebody typed. This record
names the board from `a020d3` and flags the hostname so that nobody later reads the log's
filename, or the SSH prompt in a pasted terminal, as a statement about the hardware. The
1.4 GHz clock is this board's stock speed, not an overclock: `config.txt` carries no
`arm_freq` line and the firmware reports a 1400000 maximum.

## Deviations from what the chapter prints

1. **The board is a 3B+ Rev 1.3, not the Pi 3 Model B Rev 1.2 the chapter allocates.** Closer
   than the Pi 4B used before it, and the right memory class, but not the same part: the 3B+
   has the BCM2837B0 at 1.4 GHz where the 3B has the BCM2837 at 1.2 GHz.
2. **No LEDs.** The three modules stayed wired to the Pi 4B, so this run drove no GPIO line
   and its actuation count of zero errors means only that nothing was attempted. The colour
   agreement is unaffected, being a comparison of two columns in a replayed stream.
3. **Run as `bing`, not through the systemd unit.**

## How the log reached this repository

`scp` from the board to the authoring laptop **after the run had exited**, then checked against
the board's own count: 201 lines and 31569 bytes, no carriage returns, matching on both sides.

Those are also the counts of every other e03 log, which is expected rather than alarming and
was verified rather than assumed: the timing fields are written to three decimals and occupy
eight characters for every value in range, `n_generated` is two characters on every row, and
`t_s` is the replay index rather than a wall clock. With the model output identical, nothing
is left that could change the length. The md5 sums differ, `d7683326db2e` here, and the
column comparison above names exactly which two columns carry the difference.
