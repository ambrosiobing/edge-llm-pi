# projects/e16-nbiot-sleep-interval/CMakeLists.txt
# The NB-IoT sleep interval on the shared loop, its rule and the deterministic battery
# function the rule calls, the rule's six test cases for the shared harness, the unit
# test of the battery function and the 3GPP timer decode, and the oracle test over the
# 100 replayed scenarios (stub builds only).
#
# RULES carries two files, not one. src/rules_e16.c is the rule, and src/battery.c is
# the arithmetic it calls; both must reach tests/rules_test as well as the executable,
# and RULES is the list edgellm_add_project collects for that harness. The quoted
# include "battery.h" in both files resolves next to the including file, so rules_test
# needs no extra include path.
#
# NO_MAIN plus the shared main listed as a source, with EDGELLM_NO_MAIN defined: the
# project's own main() in src/e16_main.c takes --at-plan, --at, --tau and --active out
# of argv and calls edgellm_main() with the rest. The loop itself is untouched.
#
# The serial backend is this project's only hardware option. The event source is always
# the replay backend, which is always compiled in: an attach and a PSM cycle take
# minutes, so nothing is decided live. What --at needs the backend for is the AT
# bring-up that reads the granted window, and src/at_cycle.c compiles that path only
# when EDGELLM_IO_SERIAL is defined, so a build without it refuses --at with the option
# it wants, which is what CI sees.

edgellm_add_project(edgellm-e16
    SOURCES src/e16_main.c src/at_cycle.c "${EDGELLM_ROOT}/common/c/src/edgellm_main.c"
    RULES src/rules_e16.c src/battery.c
    RULES_TEST src/rules_e16_test.c
    IO serial
    NO_MAIN)
target_compile_definitions(edgellm-e16 PRIVATE EDGELLM_NO_MAIN=1)
if(EDGELLM_IO_SERIAL)
    target_compile_definitions(edgellm-e16 PRIVATE EDGELLM_IO_SERIAL=1)
endif()

edgellm_add_oracle_test(edgellm-e16
    PROJECT e16
    REPLAY replay/profiles_100.csv
    GRAMMAR grammar.gbnf
    PROMPT prompt.txt
    EXPECTED "${EDGELLM_ROOT}/tests/oracle/e16.expected.csv"
    ARGS --n-predict 64 --temperature 0.3)

# The battery function and the timer decode are checked on their own, with no loop, no
# grammar and no engine, because they are the part of this project that must not be the
# model's. libm is linked for fabs() in the test itself.
if(EDGELLM_BUILD_TESTS)
    add_executable(e16_battery_test src/battery_test.c src/battery.c src/at_cycle.c)
    target_link_libraries(e16_battery_test PRIVATE edgellm_core m)
    target_compile_options(e16_battery_test PRIVATE -Wall -Wextra)
    add_test(NAME e16_battery_test COMMAND e16_battery_test)
endif()
