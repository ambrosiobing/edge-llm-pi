# projects/e03-led-explainer/CMakeLists.txt
# The LED status explainer on the shared loop, its rule and the rule's eight test cases
# for the shared harness, and the oracle test over the replayed state stream (stub
# builds only). The gpio backend is the bench's: a build without -DEDGELLM_IO_GPIO=ON
# says so at configure time and refuses --leds at run time, which is what CI does.
#
# NO_MAIN plus the shared main listed as a source, with EDGELLM_NO_MAIN defined: the
# project's own main() in src/e03_main.c takes --leds <spec> out of argv, opens the gpio
# sink, and calls edgellm_main() with the rest. The loop itself is untouched.

edgellm_add_project(edgellm-e03
    SOURCES src/e03_main.c "${EDGELLM_ROOT}/common/c/src/edgellm_main.c"
    RULES src/rules_e03.c
    RULES_TEST src/rules_e03_test.c
    IO gpio
    NO_MAIN)
target_compile_definitions(edgellm-e03 PRIVATE EDGELLM_NO_MAIN=1)

edgellm_add_oracle_test(edgellm-e03
    PROJECT e03
    REPLAY replay/state_200.csv
    GRAMMAR grammar.gbnf
    PROMPT prompt.txt
    EXPECTED "${EDGELLM_ROOT}/tests/oracle/e03.expected.csv"
    ARGS --n-predict 64 --temperature 0.3)
