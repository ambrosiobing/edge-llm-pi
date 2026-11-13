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
costs **0.059 us per call**, fifty-nine nanoseconds, the median over six runs of 2000 calls
with a sample sd of 0.021. Against a decision of 15.4 seconds that is four parts in a
billion. That is the question this chapter was built around and it is
answered, and answering it needed no model at all.

## THE BOUNDARY, FROM THE STUB, WHICH IS THE INSTRUMENT FOR IT

With the grammar-only stub behind the same four functions the decision costs nothing
measurable, so the difference between the two arms is the boundary alone.

Six runs of 2000 calls each. The first two were taken before the tool printed medians, so
they have a mean-based figure only.

    run   boundary by the means   boundary by the medians
      1          0.064 us                 not printed
      2          0.092 us                 not printed
      3          0.114 us                    0.074 us
      4          0.071 us                    0.092 us
      5         -0.095 us                    0.038 us
      6          0.011 us                    0.037 us
      7          0.078 us                    0.056 us
      8          0.063 us                    0.056 us

    by the medians, runs 3 to 8   0.059 us  sd 0.021  from 0.037 to 0.092
    by the means,   runs 3 to 8   0.040 us  sd 0.074  from -0.095 to 0.114

**THE MEAN IS THE WRONG STATISTIC HERE AND IT SAYS SO ITSELF: in run 5 it is negative.**
`edgellm_abi_decide` only adds work to the call it forwards, so it cannot cost less than
nothing. In that run the outlier happened to land in the in-process arm, whose maximum was
121.851 us. The median-based figure is never negative and is three and a half times less
variable.

**The cause is visible once the maximum is printed.** In run 3 the ABI arm's median is
0.296 us and its maximum is 100.166 us, 338 times the median. One call of 100 us in 2000
lifts the mean by 0.05 us, which is the whole distance between that run's mean of 0.340 and
its median of 0.296. That is the scheduler, not the seam.

Each arm's own median is steadier than their difference: 0.278 to 0.297 us through the ABI
over six runs, and 0.204 to 0.259 us in process.

**And a prediction of mine was refuted on the way.** Before the five repeat runs I wrote that
the medians would fall between 0.06 and 0.09 us. Three of the five are below 0.06, and the
spread is a factor of 2.5. What held was the comparison the change was made for, that the
median varies far less than the mean, and that is the claim the tool was altered on.

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

    established   the boundary is 0.059 us by the medians, sd 0.021, over six runs of
                  2000 calls, which is four parts in a billion of a 15.4 s decision
    established   the magnitude follows from abi.c: a mutex pair and a small copy
    established   one run of this test cannot give the boundary to three decimals; the
                  run-to-run spread is a factor of 2.5 and the within-run error 1 per cent
    established   the mean-based boundary goes negative, which is impossible, so the
                  median is the statistic for this measurement
    REFUTED       my own prediction that the six medians would fall in 0.06 to 0.09 us;
                  three fell below it
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
    established   the outlier is a preemption and not the library: with the maximum now
                  printed, run 3's ABI arm shows a median of 0.296 us against a maximum of
                  100.166 us, and run 5's in-process arm a maximum of 121.851 us. Both arms
                  catch them, which is what a scheduler does and what a wrapper cannot
