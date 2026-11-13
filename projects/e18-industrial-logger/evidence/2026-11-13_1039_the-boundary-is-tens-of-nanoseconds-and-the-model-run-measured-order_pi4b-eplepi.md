# e18: the ABI boundary costs tens of nanoseconds, and the model run measured block order,
# Friday 13 November 2026, 10:39

    board      Raspberry Pi 4 Model B Rev 1.4, 8 GB, Cortex-A72, card eplepi
    instrument projects/e18-industrial-logger/c/abi_test.c, the --calls measurement
    builds     build-e18c, the real engine against the submodule's libllama.so
               build-e18c-stub, configured -DEDGELLM_STUB_MODEL=ON, no llama.cpp
    clock      the board read Fri 13 Nov 10:39:23 CET 2026 in this sitting; the model
               run's own log ~/e18_abi_200.txt is stamped 2026-11-13_1004
    thermal    54.5 C, get_throttled 0xe0000, three sticky bits already set before any
               of this; see WHAT THIS DOES NOT ESTABLISH

**THE SEAM IS FREE.** The C ABI between the Rust device service and the C inference daemon
costs **tens of nanoseconds per call** against a decision of 15.4 seconds, which is four to
six parts in a billion. That is the question this chapter was built around and it is
answered, and answering it needed no model at all.

## THE BOUNDARY, FROM THE STUB, WHICH IS THE INSTRUMENT FOR IT

With the grammar-only stub behind the same four functions the decision costs nothing
measurable, so the difference between the two arms is the boundary alone.

    run one, 2000 calls    through the ABI  0.286 us  sd 0.017
                           in process       0.222 us  sd 0.021
                           boundary         0.064 us

    run two, 2000 calls    through the ABI  0.318 us  sd 0.481
                           in process       0.226 us  sd 0.023
                           boundary         0.092 us

Same binary, same board, minutes apart. **The figure moved 44 per cent and the ABI arm's
sample standard deviation moved by a factor of 28.** A mean rising 0.032 us while the sd
rises 0.464 over 2000 calls is one call of roughly 20 us: the scheduler taking the thread
away, not the boundary.

So the honest statement is a range and not three decimals. The within-run standard error of
the first run is 0.0006 us and it is **not** the uncertainty of this measurement; the
run-to-run spread is two orders of magnitude larger than it. This is the reason the tool now
prints the median and the maximum of each arm and gives the boundary by both the means and
the medians.

**And the magnitude is exactly what the code predicts.** `edgellm_abi_decide` in
`projects/e18-industrial-logger/c/abi.c` does two null checks, one uncontended mutex pair,
the forwarded call to the same `edgellm_decide` the other arm calls directly, a `strlen` and
a `memcpy` of the answer, 36 bytes with the stub. Tens of nanoseconds is what that costs. A
mechanism read from the source predicted the order of magnitude before the number arrived.

## THE MODEL RUN, WHICH DOES NOT MEASURE THE BOUNDARY

    200 calls, qwen2.5-0.5b-instruct-q4_k_m, 104 m 16 s wall

    through the ABI   mean 15 680 157 us   sd 547 282 us
    in process        mean 15 385 790 us   sd 453 777 us
    printed as "boundary per call"    294 367 us

    standard error of the difference, unpaired:  50 271 us
    the difference in standard errors:           5.9
    the difference against the stub's boundary:  4 600 000 times
    the difference as a share of one call:       1.88 per cent

**The difference is real and it measures the wrong thing.** The test runs its two arms as
blocks, every call through the ABI before any call in process, because two engines do not
fit on the board. The first block pays for paging in 462.96 MiB and for the governor ramping
and the second block finds both done. Thermal throttling is excluded as the explanation
because it would slow the second block, and the second block is the faster one.

A figure can be 5.9 standard errors from zero and still be an artefact of the design. That
is the finding worth keeping from this half of the sitting.

The header of `abi_test.c` claimed "the difference of the two means is the cost of the
boundary per call" without qualification. It is corrected, and the tool now prints the
standard error of the difference always and refuses to label the difference a boundary when
a model is loaded.

## WHAT THIS DOES NOT ESTABLISH

    established   the boundary is tens of nanoseconds: 0.064 us and 0.092 us on two runs
                  of 2000 calls, against a decision of 15.4 s
    established   the magnitude follows from abi.c: a mutex pair and a small copy
    established   one run of this test cannot give the boundary to three decimals; the
                  run-to-run spread is 44 per cent and the within-run error 1 per cent
    established   the model run's 294 367 us is block order, not a seam, and is 4.6
                  million times the boundary measured on the same host
    established   the cost of a decision through the ABI on this board is about 15.4 s
    established   the three ABI checks pass against the real engine and against the stub
    NOT CLOSED    acceptance clause three. The chapter requires all four latency rows from
                  ONE sitting with get_throttled reading 0x0 before and after, and a run
                  with any bit set discarded. This sitting was never read before it, read
                  0xe0000 after, and the FFI and socket rows were not run at all. Sticky
                  bits clear only at boot, so that sitting has to start from a reboot.
    NOT MEASURED  the FFI and socket boundaries, which need edgellm-e18 --bench
    NOT SHOWN     that the 20 us outlier is the scheduler rather than something in the
                  library; the median and maximum now printed will show it next time
