# e18: skyhorizon builds it clean and holds the wrong model, Monday 9 November 2026, 10:40

    host       win11 skyhorizon, WSL side, clone ~/src/edge-llm-pi. NO BOARD
    subject    the two questions clause two was waiting on, asked as one command each
    tree       9f8cbab, reached by a fast forward from 6d0f89c
    toolchain  cargo 1.99.0 against this manifest's rust-version floor of 1.75

**THIS CLOSES NO CLAUSE.** It answers the NOT CHECKED line carried by the records of 08:44 and
09:16, and it turns clause two from blocked on an unknown into blocked on a decision.

## THE SUITE REPRODUCES project.toml EXACTLY

    22 passed, 0 failed    the unit tests
     2 passed, 0 failed    the oracle tests, including the 600-frame replay across the ABI

    e18: 600 frames, 0 refused, 0 sequence gaps, 600 answers, 0 engine errors,
         0 over the 2.0 s period, mean call 0.0 ms, backend stub, abi 1000

`project.toml` has recorded 22 and 2 since Sunday 18 October 2026 and both numbers hold under a
toolchain 24 minor versions above the declared floor. **This is a re-check that found no
drift**, which is the outcome worth recording precisely because the record was accurate.

## AND THE MODEL ON THIS HOST IS THE 1.5B, NOT THE 0.5B

Every `.gguf` on the host, largest first, excluding the vocabulary fixtures that ship inside
`third_party/llama.cpp/models`:

    1117320736 bytes   /home/bing/models/qwen2.5-1.5b-instruct-q4_k_m.gguf

That is the whole list. **There is no file at 491400032 bytes**, which is the size board A
carries for `qwen2.5-0.5b-instruct-q4_k_m.gguf`. So the two hosts hold different models, and
neither host holds both halves of what clause two needs:

    board A, Pi 4B on card eplepi    the engine, and the 0.5B at 491400032 bytes.
                                     NO Rust toolchain, so the service cannot be built
    skyhorizon, WSL                  cargo 1.99.0, a clean build and a passing suite.
                                     The 1.5B only, which is a different model

**Running the table here with the 1.5B would produce a real number that answers a different
question**, since the volume's claim is about what a Pi runs, and a figure from a model three
times the size is not comparable with any other figure in the volume. That is a worse outcome
than no figure, because it would sit in the same table as the rest.

## WHAT THIS LEAVES, STATED AS A DECISION AND NOT A BLOCKER

    option   put a Rust toolchain on board A and run clause two there. The engine and the
             0.5B are already on it. This is the option that keeps the figure comparable
    option   copy the 0.5B onto skyhorizon and run the table there. Valid for agreement,
             which does not depend on the host, and it yields NO latency figure for the Pi
    option   leave clause two pending and say so in the chapter, which is what the
             chapter now does

The second needs care in how it is published, because an agreement table taken off the board
is still an agreement table, and the volume has a column for the instrument that would have to
say so.

## WHAT THIS DOES NOT ESTABLISH

    established   the Rust half builds and passes 22 plus 2 on skyhorizon at cargo 1.99.0
    established   skyhorizon holds the 1.5B at 1117320736 bytes and no 0.5B
    established   no single host currently holds cargo, the engine and the 0.5B together
    NOT shown     anything about any model's answers on this stream
    NOT shown     clause one, the 1000-frame echo, which needs the WS-28164 on card B
    NOT a blocker the eight interface errors recorded in project.toml, which closed on
                  Sunday 18 October 2026 and did not recur here
