# e17: the parser's eleven cases and the rule's six, Friday 30 October 2026, 07:33

    board      Raspberry Pi 3 Model B Plus Rev 1.3, hostname rpi3BV1p2, 905 MB,
               Cortex-A53, card Raspberry Pi OS Debian 13 trixie, kernel 6.18.50
    tree       9dc8c53, which is origin/main, twenty-two commits behind the authoring
               laptop's local HEAD; why that is sound is argued below
    toolchain  cmake 3.31.6, gcc (Debian 14.2.0-19) 14.2.0
    configure  -DEDGELLM_STUB_MODEL=ON -DEDGELLM_BUILD_TESTS=ON -DEDGELLM_BUILD_LLAMA_TOOLS=OFF
    engine     none built and none linked; neither test touches a model
    register   0x0 before, 0x0 after; 32.7 C at boot, 38.1 C after the build, 35.9 C at rest

**This closes the clause e17's acceptance puts first and nothing else.** The acceptance reads
"1440 samples at one a minute with a model answer every 15 minutes, so 96 answered samples;
the state agreement over those 96; the count of invented operator names and of numbers that
were not in the inputs, published whatever they are; **the parser's eleven cases pass first**".
The eleven, and the rule's six, now pass. No sample was taken, no modem was attached and no
SIM exists.

## The two results

    e17 at_parser: 39 checks, 0 failures                                exit 0
    rules_test: 13 rules, 80 cases, 405 checks, 0 failures              exit 0

    ctest -R "e17-at-parser|rules_test"
      Test #14: e17-at-parser ...... Passed  0.00 sec
      Test #19: rules_test ......... Passed  0.01 sec
      100% tests passed, 0 tests failed out of 2

## WHY "39 CHECKS" IS NOT THE ACCEPTANCE'S "ELEVEN CASES"

The binary reports checks and the acceptance names cases. **Those are different units and the
figure that passes is not the figure the clause asks for**, so the cases were counted rather
than inferred from the checks.

    case_* functions DEFINED in at_parser_test.c    11
    case_* functions CALLED in main                 11, the same names

    case_csq_plain          case_csq_unknown        case_cereg_read
    case_urc_mid_response   case_cops_no_operator   case_cpsms
    case_quoted_operator    case_error              case_cereg_urc_is_not_an_answer
    case_timeout            case_full_round

**Defined is not called**, which is why both lists were taken and compared by name rather than
by count. All eleven run. The 39 checks are the assertions inside them, raised by `check`,
`check_int` and `check_text`, each of which increments the same counter.

The file names which cases carry the weight: case 4 is "the case the parser exists for", an
unsolicited result code arriving between a command and its answer; case 9 is the defect that
prevents, a URC being mistaken for the answer; case 10 is the timeout a modem with no answer
must produce rather than a wait that never ends; and case 11 is the full round of four queries
the sampler asks each minute.

## The rule's six, which pass inside an aggregate of eighty

`rules_test` is a shared harness: it links every project's rule cases and prints one summary,
so "0 failures" is a statement about all twenty projects and not about e17. e17's share was
counted in the source:

    rules_test_expect("e17", "registered at home with signal 18 is OK",          0)
    rules_test_expect("e17", "roaming with signal exactly 10 is OK",             0)
    rules_test_expect("e17", "registered with signal 9 is DEGRADED",             1)
    rules_test_expect("e17", "signal 99 means unknown, so DEGRADED not OK",      1)
    rules_test_expect("e17", "registration denied with signal 31 is DOWN",       2)
    rules_test_expect("e17", "a sample without a registration field is DOWN",    2)

Six, each labelled, and three of them are the ones the file flags as having "a wrong answer
that looks right": a signal of exactly 10 at the boundary, 99 meaning unknown rather than
excellent, and a truncated sample with no registration field at all.

**The harness has its own guard against the case that matters here.** If a registered case's
rule is not linked into the binary it prints `rule not linked into rules_test` and fails,
rather than passing because nothing ran. No such line appeared, so e17's six were linked and
executed rather than silently absent. The thirteen rules are the twelve projects declaring a
`RULES_TEST` plus the smoke rule.

## TWO TRAPS THIS RUN WAS SHAPED TO AVOID, AND ONE OF THEM ANNOUNCED ITSELF

**This repository drops targets silently when an optional dependency is missing**: the
configure still succeeds and the target is simply absent, so a build that "passed" can have
built nothing. The tests were therefore **listed before they were built**:

    ctest --test-dir build-tests -N
      Test #13: edgellm-e17-oracle
      Test #14: e17-at-parser
      Test #19: rules_test

The `Could not find executable` lines that accompany that listing are `ctest -N` resolving
paths for binaries that do not exist yet, which is what a configure with no build looks like.

And the same configure log shows the drop happening to a different project, out loud:

    -- edgellm-e19: backend iio not compiled in; the bench build needs -DEDGELLM_IO_IIO=ON

So the mechanism is real and visible on this very run. e17's two targets were registered; e19's
was not, and said so.

**No engine was built.** `-DEDGELLM_STUB_MODEL=ON` serves the engine header from the
grammar-only stub, so llama.cpp was never configured, never cloned into and never compiled.
That matters on a 905 MB board: the parser test links nothing but the parser, and the whole
build finished in a couple of minutes at `-j2` with the board rising 5.4 C.

## Why a tree twenty-two commits behind is sound here, and where it is not

Board B's clone sits at `9dc8c53` while the authoring laptop carries twenty-two unpushed
commits. That was checked rather than waved through, by asking what the two binaries actually
compile:

    at_parser_test   links src/at_parser.c and nothing else
    rules_test       links edgellm_core and every projects/*/src/rules_eNN_test.c

None of the twenty-two touches `at_parser.c`, `rules_e17.c` or `rules_e17_test.c`. The io
backend fixes are in backends this configuration leaves off, the e04 client fix is e04's own
`main`, and e13's grammar change is not C. So the figures above are a true reading of e17's
parser and rule cases at `9dc8c53`.

**Where it would not be sound is the oracle test**, `edgellm-e17-oracle`, Test #13, which was
registered and deliberately not run here. That one replays `day_1440.csv` through the stub and
compares against `tests/oracle/e17.expected.csv`, so it depends on the grammar and the shared
loop, and both have moved on the laptop. Running it on a stale tree would produce a number
that describes neither tree.

## What is established and what is not

    established   the parser's eleven cases run and pass, 39 checks, 0 failures
    established   the rule's six cases run and pass inside 80 cases, 405 checks, 0 failures
    established   both are registered with ctest and pass under it
    established   the board stayed at 0x0 and under 39 C throughout
    NOT shown     any of e17's three acceptance quantities
    NOT shown     the 1440 samples, which need the modem and a Cat-M or NB-IoT SIM
    NOT shown     the state agreement over 96 answered samples
    NOT shown     the count of invented operator names, which is this chapter's own figure
    NOT run       edgellm-e17-oracle, which this tree is too old to score honestly

## What e17 is actually waiting for

**Not cooling.** This is worth stating because four other clauses in the volume are blocked on
a fan and e17 is not one of them. Its duty cycle is one model answer every fifteen minutes
across 1440 samples, so the board cools completely between answers, nothing like e06's 87
percent. Its acceptance carries no `get_throttled` clause either.

**A Cat-M or NB-IoT SIM**, which the chapter itself lists as an open item: "without one this
chapter stops at AT bring-up and says so". Also the SIM7070G HAT seated with its LTE antenna,
`dtoverlay=disable-bt` to free the PL011 UART on header pins 8 and 10, and the host logic
voltage read off the HAT's 0 ohm resistor before power is applied.
