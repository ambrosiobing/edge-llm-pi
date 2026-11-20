# e18: acceptance clause three is met. Four latency rows from one sitting that began at a
# reboot, throttle 0x0 before and after every repetition, one budget on all four arms.
# Thursday 19 November 2026, 18:11 to 21:41

    board      Raspberry Pi 4 Model B, 8 GB, card eplepi, commit 10c09bb
    builds     build-e18c, the real engine; edgellm-e18 with --features ffi
    model      qwen2.5-0.5b-instruct-q4_k_m.gguf, budget 128 on all four arms
    daemon     PID 988, started once at 18:12, one cold load, alive for the whole sitting
    logs       on the board, not in this repository: /home/bing/e18_r1_abi.log,
               e18_r1_ffi.log, e18_r1_sock.log and the same for r2 and r3, plus
               e18_daemon.log. Each is thousands of lines of engine output around four
               result lines, so the result lines are quoted below and the files stay
               where they were written.
    thermal    0x0 at 51.1 C after the reboot; 0x0 at every one of the eight readings;
               0x0 at 57.4 C at the end. Peak 61.8 C.

## THE FOUR ROWS, MEDIANS OF FIVE CALLS, THREE REPETITIONS

                            r1        r2        r3     median of the three
    through the ABI       15.652    15.260    15.264        15.264 s
    in process            15.410    15.508    15.245        15.410 s
    over the FFI          15.877    15.706    15.902        15.877 s
    over the Unix socket  15.801    15.685    15.425        15.685 s

    ABI minus in process  +0.437    -0.117    +0.030

**All twelve medians lie between 15.245 and 15.902 s: a spread of 0.657 s, 4.2 per cent.**
The four paths are not distinguishable at this resolution, which is the claim this chapter
was built around, and it now has four rows from one qualifying sitting behind it.

## THE DIFFERENCE CHANGES SIGN TWICE, AND ONLY THE REPETITION SHOWS IT

Plus 437 ms, minus 117 ms, plus 30 ms. **A wrapper that only adds work to the call it
forwards cannot make that call faster, so a negative value is impossible as a boundary and
the difference is noise at this resolution.** Same logic as the stub's mean going negative on
Friday 13 November 2026.

**One repetition would have published the first of the three.** It reads +0.437 s at 1.19
standard errors, which is the kind of figure a reader turns into a tendency. The second
repetition refutes it by sign. This is the first time this project's insistence on repetition
has paid in a measurement rather than in an argument for one.

**And it weakens an explanation of mine from Friday 13 November 2026.** There the first block
was always the slower one and I attributed it to paging in 463 MiB and the governor ramp. Here
the ABI block ran first in all three repetitions and was slower in one and faster in two. At
five calls the block-order effect is below the noise as well.

## WHAT THE WITHIN-RUN SPREAD SAYS, STATED PRECISELY

    within-run sample sd, the twelve arms   0.088 to 0.803 s
    run-to-run spread of the medians        0.196 to 0.392 s

The within-run standard error is the sd over the square root of five, so 0.04 to 0.36 s
against a run-to-run spread of 0.20 to 0.39 s. **They are the same order here.** This chapter
says elsewhere that the within-run standard error understates the run-to-run spread by two
orders of magnitude; that was measured on the stub at 2000 calls, where the standard error is
tiny, and **it does not reproduce at five calls with a model.** The reason to repeat stands on
the sign change above, not on that ratio.

## THE THERMAL WINDOW IS TWENTY-THREE MINUTES OF LOAD, NOT OF WALL CLOCK

The sitting ran from 18:11 to 21:41, **three and a half hours of wall clock**, and the board
peaked at 61.8 C and never set a bit. Load was about eighteen minutes of it, spread across
three repetitions with idle gaps between them while the output was read.

**So my own sizing of this sitting was conservative for a reason I had not separated.** The
23-minute figure, measured Friday 13 November 2026, is 23 minutes of *continuous* load to
80 C. With gaps the board dissipates between repetitions. Five calls per arm fitted easily;
whether twenty would fit when the repetitions are spaced is **not measured**, and the honest
statement is that the window is a load budget and this sitting never approached it.

## COMPARED WITH THE SITTING OF FRIDAY 13 NOVEMBER 2026, AND A CORRECTION OF MINE

That sitting measured all four arms at 20 calls three times. **It could not satisfy this
clause for two reasons**: it ran at about 80 C with throttle bits set, and its four rows were
taken at two different budgets, the C arms at 64 and the socket arm at the daemon's 128, which
was found and fixed earlier today.

    Friday 13 November 2026, about 80 C    socket 17.855 s    ffi 18.180 s   medians
    Thursday 19 November 2026, 48 to 62 C  socket 15.685 s    ffi 15.877 s   medians

**2.3 s, fifteen per cent.** The chapter attributes 4.0 to 4.7 per cent to throttling, which
covers part of it; the rest is not explained here and is not claimed.

**The correction: I spent this evening stating that the FFI and socket arms had never printed
a line.** They printed theirs on Friday 13 November 2026 at 17:23 and the figures have been in
the chapter's table ever since. I read `NOT MEASURED the FFI and socket boundaries` in the
10:39 record of that day, which was true at 10:39 and superseded by the same day's second
sitting, and I never read the 17:23 record beside it, which says in so many words that those
arms had never been run *before that day*. The chapter's table carried two stale rows beside
the measured ones, saying those boundaries were not measured at a call count nothing uses;
they are removed with this record. **A record is
true at its timestamp and the next record is the one that supersedes it; reading the earlier
of two records from one day is reading the stale one.**

## AN UNCONTROLLED PROCESS ON THE MEASUREMENT HOST, FOUND AFTER THE SITTING

At 00:10 on Friday 20 November 2026, with the daemon stopped, `free` still read 2677 MB used
against 955 MB a minute after the reboot. The cause is not this project:

    RSS 2 433 916 kB   PID 842   freqtrade
    RSS   131 844 kB   PID 885   python
    RSS    77 388 kB   PID 841   node

**A Freqtrade bot with a 2.32 GB resident set runs on this board and ran through both latency
sittings**, this one and that of Friday 13 November 2026. It read 955 MB used one minute after
the reboot because it had just started, and grew to 2.3 GB over the three and a half hours.
`/run` holds 101 MB, so the daemon's state file is not the cause.

**This does not invalidate the four rows**: all four arms ran under the same conditions within
each repetition, the within-run spread was 0.088 to 0.803 s, and the model loaded without
pressure, 5142 MB still available at the end. **What it does is name a confounder that no
record of this volume had named.** The bot competes for CPU and for page cache with every
measurement taken on board A, its share during the sitting was not measured, and its footprint
differed between the two sittings, so it is a candidate for the unexplained part of the
fifteen per cent above. Whether it is stopped for future sittings is Joseph's decision; what is
not optional is that it appears in the conditions of any figure measured on this board.

## WHAT THIS DOES NOT ESTABLISH

    established   clause three is MET: four rows, one sitting, begun at a reboot, 0x0 before
                  and after every repetition, one budget of 128 on all four arms
    established   the four paths lie within 4.2 per cent of each other, twelve medians
                  from 15.245 to 15.902 s
    established   the ABI minus in-process difference changes sign twice, so it is noise
    established   the budget line and the socket arm's refusal to print a number both work
    established   a binary built with --features ffi needs the C library on the link path
                  in EVERY arm, the socket arm included, which never calls it
    NOT SHOWN     that any of the four paths is faster than any other
    NOT EXPLAINED the 2.3 s between this sitting and that of Friday 13 November 2026;
                  throttling covers 4 to 5 points of the fifteen
    NOT CONTROLLED  a Freqtrade bot with a 2.32 GB resident set on the same board,
                  through both sittings; its CPU share was never measured
    NOT MEASURED  whether twenty calls per arm fits when the repetitions are spaced
    NOT CLOSED    clause one, the echo, which needs the WS-28164 on card B
