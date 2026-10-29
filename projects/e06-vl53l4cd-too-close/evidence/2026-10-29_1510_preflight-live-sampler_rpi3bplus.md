# e06 pre-flight: the ULD, the sensor and the loop, Thursday 29 October 2026, 15:10

    board      Raspberry Pi 3 Model B Plus Rev 1.3, revision code a020d3, 905 MB,
               Cortex-A53 at 1.4 GHz, card Raspberry Pi OS Debian 13 trixie, user bing
    sensor     Adafruit VL53L4CD breakout, ADA5396, single zone, alone on I2C bus 1
    driver     ST ULD STSW-IMG026, package VL53L4CD_ULD_V2.2.3, out of tree at
               /home/bing/uld/, licence SLA0103
    model      SmolLM2-360M-Instruct-Q4_K_M.gguf
    engine     llama.cpp v0.6.0, the pinned tag, built on this board
    also       a replay rehearsal on the Pi 4B earlier the same day

**This closes no clause of e06 and is not a measurement of the project.** Every one of e06's
three acceptance quantities comes from the sensor during 200 hand approaches, and no approach
has been made. What this records is that the ULD, the sensor and the loop are now in place,
and four things found while putting them there, three of which change how the afternoon should
be run.

**AND ONE THING IS STILL MISSING BESIDES THE HAND-WORK, FOUND AFTER THIS RECORD WAS FIRST
WRITTEN.** An earlier version of this paragraph said everything except the hand-work was in
place. It is not: **e06 cannot pass its own gate on this bench without cooling.**

e06's acceptance ends "All three are taken with `vcgencmd get_throttled` reading `0x0` before
and after, and a run with any bit set is discarded." Its loop is **near-continuous inference**
rather than an idle board sampling occasionally: run 5 above produced 8 answers of 12.24 s
inside 112 seconds of wall clock, an **87 percent duty cycle**, so it heats the way
`llama-bench` does.

    board A    thermal limit after 367 s of load, 6.1 minutes
    board B    thermal limit after 282 s of load, 4.7 minutes

    200 approaches at 200 mm/s, the fastest plausible    about 10 minutes
    200 approaches at 100 mm/s                           about 17 minutes
    200 approaches at  50 mm/s                           about 30 minutes

Splitting into one session per speed does not rescue it. Only the fastest speed would fit
inside board A's 6.1 minutes, and barely, while the slow speed that actually brackets the
latency threshold runs to fifteen minutes on its own. **There is no fan on this bench**,
stated Thursday 29 October 2026, so this is a purchase and not a test.

So e06 joins e01's two cells and e03's four-thread clause behind the same part. That is four
clauses, and it was worth finding before two hundred hand approaches were made into a run that
e06's own acceptance discards.

## The ULD, which was the blocker, and its licence

e06's CMake takes the vendor driver from outside the tree through
`EDGELLM_VL53L4CD_ULD_DIR`, and the project's README says it is "never vendored until its
licence is read". It is read.

    LICENSE.txt   "...applicable license terms are in the Package_license file. If you
                   received this software component outside of a package or without
                   applicable license terms, the terms of the BSD OPEN SOURCE SLA0103
                   license shall apply."

**There is no `Package_license` file in the package**, so by its own fallback clause SLA0103
applies. SLA0103 is ST's "BSD OPEN SOURCE" licence, published as the clear BSD licence:
source redistribution retains the copyright notice, the condition list and the disclaimer,
and no patent rights are granted. **So vendoring into this public repository would be
permitted.** It is not done anyway: the out-of-tree arrangement already exists and works, and
permission is not a reason.

The package is version **2.2.3**, newer than the 2.2.2 carried by the `stm32duino/VL53L4CD`
Arduino mirror, which is also BSD-3-Clause but renames ST's C to `.cpp` and replaces
`platform.h` with a C++ class, so e06's CMake rejects it. A third mirror found during the
search carries no LICENSE file at all and is not a route.

**One assembly step the package does not do for you.** The four files e06 globs for live in
two different directories, and the CMake refuses unless all four sit together:

    VL53L4CD_ULD_V2.2.3/VL53L4CD_ULD_Driver/   VL53L4CD_api.c  VL53L4CD_api.h  VL53L4CD_calibration.c
    VL53L4CD_ULD_V2.2.3/Platform/              platform.h

Copying `platform.h` beside the driver is enough, after which the configure reports
`edgellm-e06: live sampler on, ULD from ..., header VL53L4CD_api.h, wait VL53L4CD_WaitMs`
and the two vendor C files compile into the target with no warnings.

## The sensor, wired and verified

    header 1   3V3            VL53L4CD VIN
    header 3   GPIO2, SDA1    VL53L4CD SDA
    header 5   GPIO3, SCL1    VL53L4CD SCL
    header 9   GND            VL53L4CD GND
               XSHUT and GPIO1 unconnected: no shutdown, no interrupt

`i2cdetect -y 1` returns **0x29 and nothing else**. The X-NUCLEO-IKS5A1 that had been on this
bus was removed first, deliberately: the two would not have collided, since its parts answer
at 0x19, 0x1e, 0x5c, 0x6a and 0x6b, but **on a bare bus a single address at 0x29 is
unambiguous proof of the four leads**, where among six other devices a mis-wired sensor looks
exactly like a correctly wired silent one.

I2C is not enabled on a fresh Raspberry Pi OS card. `config.txt` carried only
`dtparam=audio=on` and the sole adapter was `i2c-2`, the HDMI DDC bus. `dtparam=i2c_arm=on`
plus `i2c-dev` in `/etc/modules` and a reboot creates `/dev/i2c-1`; the controller and the
device node are two separate things.

## The loop, proven end to end

    1000 events, 8 model answers, 0 model errors, 0 actuation errors
    mean prompt 5066.8 ms, mean generate 7172.5 ms, rss 352716 kB, four threads
    vl53l4cd: 1120 samples, 0 with a range status other than 0

Zero bad range statuses across 1120 samples, and the ranges track a real object. The rule
transitions, the grammar constrains, the log takes its shape, and `tools/agreement.py` reads
it. **Nothing in e06's software path is unproven any more.**

## THE FRESHNESS GATE SETS THE SHAPE OF THE AFTERNOON

e06 does not ask the model on every sample. Its README: the loop is synchronous, a decision
takes seconds, and while the engine answers the sampler keeps writing ten rows a second into
the pipe; when the loop catches up it runs the rule on each of those rows and **asks the model
only on a sample still fresh, at most 2.5 sampling intervals old**. A row the model was not
asked about has an empty model column, which the agreement tool counts separately and never as
agreement.

Measured here rather than assumed:

    1000 events  ->  8 model answers        125 samples per answer
    992 of 1000 rows have an empty model column

**CORRECTION, made while planning the afternoon.** An earlier version of this section read
"the acceptance's two hundred model-answered rows" and sized the session from that. The
acceptance asks for **two hundred APPROACHES**, about seventy at each of three speeds; the
model-answered rows are whatever falls out of how long those approaches take. The two are not
the same number and the mistake inflated the session estimate.

**The session length is set by the approaches.** One approach is the hand crossing 600 mm at
the chosen speed, plus a reset, so its duration is `600 / v` and the speeds chosen decide the
afternoon:

    speed v     one crossing    200 approaches, crossings alone
    47 mm/s     12.8 s          about 43 minutes
    100 mm/s     6.0 s          about 20 minutes
    200 mm/s     3.0 s          about 10 minutes

**The model-answered rows then follow from the sampling**, at one per 125 samples, which is
one per 12.5 s on this board. Over a session of total length T the model answers about
`T / 12.5 s` times regardless of how the approaches are paced inside it.

That gives a useful coincidence at the slow end: at 47 mm/s an approach takes 12.8 s and an
answer arrives every 12.5 s, so **roughly one answer per approach**. At 200 mm/s an approach
takes 3 s and the model still answers only every 12.5 s, so about one approach in four carries
one. The fast speeds therefore contribute fewer scored rows than the slow ones even at equal
approach counts, and the agreement table should say so rather than let the reader assume
seventy scored rows per speed.

**The third acceptance quantity is now predictable too.** It asks the chapter to state the
speed above which `600 / v < 0.4 + L`. With L measured, that speed is **47 mm/s on this
board**, a hand taking 12.6 seconds to cross 600 mm. The three approach speeds should bracket
that number rather than be chosen blind, or all three will sit above it and the chapter will
report only that the model never kept up, which is true and uninformative.

## A PREDICTION, ON RECORD BEFORE THE MEASUREMENT

**The model answers APPROACHING regardless of the input.** Twice, on two different
distributions:

    replay, 50 events, Pi 4B    model APPROACHING x50; rule CLEAR 33, APPROACHING 17
    live, 8 answers, Pi 3B+     model APPROACHING x8;  rule TOO_CLOSE x8

The live eight were taken with a stationary object about 40 mm from the sensor. Nothing was
approaching and 40 mm is far inside the 300 mm line, and the model said APPROACHING anyway.

So **predict an agreement figure resembling e04's 42.2 percent rather than e03's 100.0**, with
the rate approximating whatever share of the rule's decisions happen to be APPROACHING. e04
scored 42.2 percent and never once emitted its third class across fifty-one events that called
for it; this is the second project to show the same collapse, and writing it down beforehand
is worth more than observing it afterwards.

That does not argue against running the afternoon. e04 is the most useful chapter in the
volume because it failed, and it priced the volume's safety rule. What it does mean is that
the interesting measurement is no longer the agreement rate but **which** literal the model
collapses to and whether approach speed moves it at all.

## A TRAP IN THE INSTRUMENT, WHICH THE INSTRUMENT DOCUMENTS ITSELF

`agreement.py` reported `not a decision of the rule 8` for those eight valid `APPROACHING`
answers. It is not a defect. The tool builds its vocabulary from the decisions **the rule
emitted in that log**, and with a stationary object the rule never left `TOO_CLOSE`, so a
valid literal fell outside the vocabulary. Its own source says exactly this:

    "the limit of that choice is a log whose rule never used one of its names, where a model
     answering that name would be reported as malformed. With the thousand-row replays in this
     volume all three names appear, and a log short enough for the risk to be real is short
     enough to read by eye."

Two hundred approaches at three speeds will exercise all three names and the trap will not
fire. It is recorded because a short diagnostic run will hit it again, and because the
contrast is instructive: **this tool documents its blind spot at the point of the blind spot**,
where e04's client printed `no literal 0` and documented nothing, which hid 136 unclosed
objects for a week.

## What is established and what is not

    established   the ULD builds, its licence is read, and it is SLA0103 clear BSD
    established   the sensor answers at 0x29 alone and returns 1120 valid samples
    established   the loop, rule, grammar, log and agreement tool work end to end live
    established   L = 12.24 s on this board, so the speed threshold is 47 mm/s
    established   one model answer per 125 samples, so 200 answers need 42 minutes here
    predicted     the model collapses to APPROACHING; agreement will resemble e04's
    NOT shown     any of e06's three acceptance quantities, none of which was exercised
    NOT shown     the repeatability at 200 mm, which needs 100 still samples
    NOT shown     anything about approach speed, since no approach was made

## Deviations from what the chapter prints

1. **The board is a 3B+ Rev 1.3, not the Pi 3 Model B Rev 1.2 the chapter allocates.** The
   right memory class and the right bus; a faster core. Which board carries the afternoon is
   still open, and the Pi 4B would halve both the latency and the session length at the cost
   of a stated deviation, as e03 did.
2. **The chapter's own dry run uses `--stub`**, and this board is built with the real engine,
   which refuses that flag and says so. The real model was used instead, which is a closer
   rehearsal: a stub cannot truncate an object or collapse to one literal.
3. **An earlier rehearsal used `--max-events 10` and produced zero model answers**, which
   looked like a fault and was not. Ten events could not drain the backlog the model's own
   twenty-five second load had already built in the pipe, so every row the loop saw was stale
   and the freshness gate correctly asked about none of them.
