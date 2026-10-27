# The under-voltage is the adapter, Tuesday 27 October 2026, 19:05

    board      Raspberry Pi 4 Model B Rev 1.4, 8 GB, Cortex-A72, four threads
    card       eplepi, Raspberry Pi OS, Debian 13 trixie, user bing
    parts      the three Joy-it LK-LED10 modules on GPIO17, GPIO27 and GPIO22,
               attached and driven in every run below
    load       e03 over replay/state_200.csv, 200 events, --n-predict 64,
               --temperature 0.3, --threads 4, identical in all three runs
    logs       evidence/e03_run_repeat_whiteisy.csv,
               evidence/e03_run_black65w.csv,
               evidence/e03_throttle_repeat_whiteisy_2026-10-27.csv,
               evidence/e03_throttle_black65w_2026-10-27.csv

Board A had set the under-voltage bit on every four-thread run since Monday 26 October 2026,
and the volume's closing chapter was about to recommend buying a power supply. This is the
substitution that tests that recommendation before any money is spent. It changes one thing,
the adapter, and runs the same load three times.

**The under-voltage is the adapter, and the fix was already on the bench.**

## The two adapters, as their labels read

    white   ISY IAC-5065        USB-C PD3.0   5.0 V 3.0 A 15.0 W at the 5 V profile
                                              9 V 3 A, 12 V 3 A, 15 V 3 A, 20 V 3.25 A 65 W
    black   65W-UC01WAEB        USB PD        "Output 5V-20V 3.25 A (Max)", 65 W

A Pi 4B is a fixed 5 V sink with no power-delivery negotiation, so it only ever draws from the
5 V profile. On that profile the two labels are near enough identical: about 5.0 V and about
3 A. **Neither is the 5.1 V the Raspberry Pi supply specifies.**

## Three runs, one variable

    time    adapter   register after   prompt      generate    temp at exit
    15:03   white     0xd0000          1320.7 ms   3448.3 ms   not recorded
    18:25   white     0xd0000          1326.1 ms   3547.5 ms   79.3 C
    19:05   black     0x80000          1338.8 ms   3561.1 ms   79.3 C

    0xd0000 = bits 16, 18, 19   under-voltage, throttling, soft temperature limit
    0x80000 = bit 19 alone      soft temperature limit

**The two white runs are a repeatability check as well as a baseline**, and the volume did not
have one before: the same configuration on different boots, hours apart, agreeing to within
0.4 percent on the prompt and 2.9 on the generation and landing on the identical register
value. That gives this bench a **run-to-run noise floor of about 3 percent**, which is what
every comparison here has to clear to mean anything.

## What the traces say, which the register cannot

Both traces are 1200 samples at about 1.03 s, taken by `tools/throttle_log.sh` alongside the
run.

    white ISY    undervolt   6 samples   throttled   6   softtemp  37
                 states 0x0  0x50000  0x50005  0xd0000  0xd0008
                 39.4 to 83.7 C
    black 65 W   undervolt   0 samples   throttled   0   softtemp 101
                 states 0x0  0x80000  0x80008
                 40.9 to 83.7 C

**Zero under-voltage samples and zero throttle events on the black adapter**, at a temperature
range identical to a tenth of a degree. `dmesg` on the white run names both episodes
independently, at 954.24 s and 1186.06 s, each about six seconds, each followed by "Voltage
normalised".

The white trace also shows what an under-voltage event costs while it lasts:

    18:25:07  0x50005  71.1 C   600 MHz   under-voltage, clock at the floor
    18:25:10  0x50000  69.6 C  1800 MHz   recovered, three seconds later
    18:30:06  0xd0000  80.3 C  1800 MHz   thermal, five minutes later
    18:30:54  0xd0008  82.7 C  1775 MHz   thermal trim, about one percent

**The two faults have opposite shapes.** Under-voltage is rare and brutal, 1800 MHz to 600, a
threefold cut, twice, for a few seconds. Thermal is frequent and mild, 1800 to between 1726
and 1800, under four percent, flickering for eight minutes.

## The clause fails on events, not on harm, and that is worth saying plainly

Totalling the live bits on the white run: **49 seconds of a 975-second run were actually
limited, 5.0 percent.** Everything else in `0xd0000` is sticky bits remembering seconds that
had already passed.

Which is why the black adapter, having removed the under-voltage entirely, is **not faster**:
prompt +0.96 percent, generate +0.38, total +0.54, all inside the 3 percent noise floor above.

So `get_throttled 0x0 before and after` is an **event test rather than a degradation test**.
It failed on twelve seconds of under-voltage. That is not an argument for softening it: this
board rebooted under a `-j4` build on Friday 16 October 2026 and left two zero-byte object
files behind, and a brownout one degree worse than these is that reboot. The clause is
catching a real risk that happens to cost little wall clock. **The acceptance is unchanged.**

## Two predictions, both written down first, one of them wrong

**Wrong.** Before the black run: "this swap will not clear bit 16", reasoning that both
adapters offer about 5.0 V and 3 A on the only rail a Pi 4 uses, so they are equivalent. They
are not. A label states what a supply **offers**, not how well it **regulates**; output
impedance and bulk capacitance decide whether 5 V survives a transient and appear on neither
label. Nothing in the photographs could have settled it, which is an argument for running the
substitution rather than for reasoning about it.

**Also wrong, and worse because it was avoidable.** The 18:25 run was read as a test of the
black adapter and written up as "two different adapters, same failure". It was the white
adapter a second time. The question "which plug is fitted" had already been asked twice and
was still unanswered when the result was interpreted. A substitution experiment whose
substitution is unconfirmed is not a substitution experiment, and the conclusion drawn from it
was a replication misread as a contrast.

## What this changes

    the volume's purchase list   a 5.1 V supply is NOT needed; drop it
    board A's supply             the black 65 W adapter, from now on
    board A's remaining fault    thermal only, 0x80000, exactly as board B
    e03 at four threads          still fails the clause, now on one bit instead of three
    e04's self-heating clause    no longer blocked by brownout on either board

Board A and board B now fail identically: heat alone, on adequate supplies. The board peaks at
83.7 C against an 85 C hard limit and engages its soft limit near 80 C, so **the gap to `0x0`
is a few degrees**, which is a heatsink rather than the fan an earlier note in this session
wrongly called for after reading the Pi 3B+'s 60 C limit as if it were the Pi 4B's.

What is not yet checked is whether 1800 MHz is this board's stock clock or a line in
`config.txt`. Board B was checked and is stock. If board A is set, one line is feeding both
the current draw and the heat, and reverting it costs nothing.

## How the logs reached this repository

`scp` from the board to the authoring laptop after each run had exited. Both run logs are
201 lines and 31569 bytes like every other e03 log, with distinct md5 sums, `f5ee421f2a` for
the white repeat and `10f1cc31cb` for the black. Both carry the same 200 answers as the other
three e03 runs, byte-identical in the `model` column, so the only columns that differ anywhere
among the five are `prompt_ms` and `generate_ms`.

**The two files were first named `psu2` and `psu3`, which asserted a second and third power
supply.** There were two supplies and three runs, and the 18:25 run was the first supply
again. They were renamed to `whiteisy` and `black65w` before anything was committed, because a
filename that states the wrong condition is read years later and believed.
