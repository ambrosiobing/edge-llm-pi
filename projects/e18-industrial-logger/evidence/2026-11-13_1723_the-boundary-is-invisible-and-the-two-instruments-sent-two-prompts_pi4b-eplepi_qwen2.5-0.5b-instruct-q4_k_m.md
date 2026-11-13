# e18: the boundary is invisible, the gap was the prompt, and the board saturates in
# twenty-three minutes. Friday 13 November 2026, 16:14 to 17:23

    board      Raspberry Pi 4 Model B Rev 1.4, 8 GB, Cortex-A72, card eplepi
    model      qwen2.5-0.5b-instruct-q4_k_m.gguf, n_ctx 512, four threads,
               temperature 0.0, --n-predict 128 on every arm
    sitting    rebooted 13:16, get_throttled 0x0 at the start, warmed with three calls,
               three repetitions of four arms interleaved, 20 calls per arm
    instruments e18_abi_test for the in-process and ABI arms, edgellm-e18 --bench for
               the socket and ffi arms; both print median and maximum since this day

## THE FOUR ARMS, BY THE MEDIAN, IN SECONDS

    arm                W1       W2       W3     mean    W1 to W3
    ABI, C          15.185   15.827   15.892   15.635    +4.66%
    in process, C   15.189   15.530   15.820   15.513    +4.15%
    socket, Rust    17.635   17.589   18.342   17.855    +4.01%
    ffi, Rust       18.115   18.035   18.391   18.180    +1.52%

## ONE: THE BOUNDARY IS INVISIBLE, THREE TIMES OVER

The ABI arm minus the in-process arm, by the median: **-0.004, +0.298, +0.072 s**, mean
+0.122. The mean-based difference the tool prints with its standard error: -50.9 +/- 178.4,
+300.9 +/- 221.2, +116.6 +/- 127.7 ms. **Every one is inside one standard error of zero and
the sign changes.** Against the 0.059 us the stub measures for the same boundary, these are
noise. Calling the ABI free is now supported by two instruments that are blind where the
other sees: a stub run with no inference at all, and three repetitions with the real model.

## TWO: THE GAP WAS THE PROMPT, AND IT IS SHOWN BY INTERVENTION

The sitting split along the **instrument**, not along the boundary: the two C arms at 15.2 to
15.9 s, the two Rust arms at 17.6 to 18.4 s, a gap of **2.444 s, 15.7 per cent**, stable
across all three repetitions while the two boundaries inside each pair differ by 0.1 to 0.3.

The cause: `abi_test` sent the bare sentence and the Rust half wraps it through
`wrap_user(Template::ChatMl, BENCH_TEXT)`, while `common/c/src/edgellm.c` templates nothing.
**Two instruments, two prompts**, against a condition that says in its own words "on the same
prompt". Nobody had seen it because the socket and ffi arms had never been run before today.

Everything else matches and was checked in the source: n_ctx 512, four threads, temperature
0.0, the same budget, the same grammar file, the same prompt file, the same shared object,
and `state` NULL on both the C arms and the ffi arm.

**The intervention.** The C test was changed to send the same bytes, wrapper included, and
re-run on the same board in the same thermal regime:

    in process, C, bare prompt,    about 80 C    15.820 s
    in process, C, wrapped prompt,       79.3 C  18.307 s
    socket, Rust, wrapped prompt,  about 80 C    18.342 s
    ffi, Rust, wrapped prompt,     about 80 C    18.391 s

**+2.49 s from the one change, landing within 0.4 per cent of both Rust arms.** The gap was
2.444 s. With the same prompt all four paths agree.

## WHAT THIS DOES NOT SETTLE ABOUT THAT GAP

**The magnitude is unexplained.** The wrapper adds roughly six to ten tokens, and that
decoding them costs 2.4 seconds follows from no calculation of mine. A first prediction, that
the wrapped prompt would produce a longer answer, was **refuted**: the answer went from 133 to
138 bytes and the five are whitespace, the summary being the same sentence. The causal claim
rests on the intervention, not on a mechanism.

**It is one reading per arm on a rising board.** The confirming run started at 59.4 C and
ended at 79.3, so its first arm ran cool and is not comparable; 16.531 s for the ABI arm is
discarded for that reason. I had just written the rule that every run is warmed first and
then did not warm this one.

## THREE: THE BOARD SATURATES IN TWENTY-THREE MINUTES, AND THAT COSTS FOUR PER CENT

    16:14   56.0 C   throttled 0x0
    16:37   81.3 C   throttled 0xe0000   (soft limit reached, three sticky bits set)
    17:00   80.8 C   throttled 0xe0000
    17:23   80.3 C   throttled 0xe0000

Rebooted at 13:16, so those bits were set during this sitting and not inherited. The board
reaches the 80 C soft limit after **23 minutes of continuous inference** and then plateaus,
holding 80 to 81 C by throttling. The cost is visible in the table above: three of the four
arms slow by **4.0 to 4.7 per cent** from the first repetition to the third.

**This makes the chapter's acceptance condition unreachable on this board as written.** Four
rows from one sitting at 0x0 from beginning to end cannot be had in a sitting longer than
about twenty-three minutes without cooling or pauses. That is measured, not estimated, and it
is a question the chapter had not asked.

## WHAT THIS DOES NOT ESTABLISH

    established   the ABI boundary is invisible with the real model: three repetitions,
                  every difference inside one standard error of zero, sign changing
    established   the instrument gap was 2.444 s, 15.7 per cent, stable over three
    established   the gap is the ChatML wrapper: equalising it moved the C arm +2.49 s
                  to within 0.4 per cent of both Rust arms at the same temperature
    established   the board reaches the 80 C soft limit in 23 minutes and then plateaus
    established   sustained inference past saturation costs 4.0 to 4.7 per cent
    REFUTED       that the wrapped prompt lengthens the answer: 133 to 138 bytes, and
                  the five are whitespace
    NOT EXPLAINED why six to ten extra prompt tokens cost 2.4 seconds
    NOT SHOWN     the confirming comparison on a warmed board in thermal steady state;
                  it is one reading per arm on a board rising from 59.4 to 79.3 C
    NOT CLOSED    acceptance clause three. Repetition 1 began at 0x0; repetitions 2 and 3
                  are discarded by the condition, and all three compared unequal prompts.
                  A qualifying sitting needs the equalised prompt, a warm-up, and either
                  cooling or arms short enough to finish inside twenty-three minutes.
