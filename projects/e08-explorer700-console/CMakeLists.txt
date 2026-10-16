# projects/e08-explorer700-console/CMakeLists.txt
# Two executables, catalogue variant O: edgellm-e08-daemon owns the model behind a Unix
# domain socket, edgellm-e08-console owns the Explorer700 HAT and asks it. Neither uses
# the shared main (NO_MAIN): the daemon has no event loop and the console's loop is
# driven by the joystick. The console's rule and its six cases go to the shared rules_test
# harness as in every project. The oracle test (stub builds only) runs the console over
# its replay file with --daemon, so the console starts the daemon itself on a socket in a
# temporary directory, and compares the log against tests/oracle/e08.expected.csv.
#
# The live half of the console (gpiod, the framebuffer, sysfs) compiles only when the
# libgpiod 2 backend is on, -DEDGELLM_IO_GPIO=ON, which is what the bench build and CI
# pass; without it the console builds replay-only and says so at --live.

edgellm_add_project(edgellm-e08-daemon
    SOURCES src/daemon.c
    NO_MAIN)

edgellm_add_project(edgellm-e08-console
    SOURCES src/console.c
    RULES src/rules_e08.c
    RULES_TEST src/rules_e08_test.c
    IO gpio
    NO_MAIN)

if(EDGELLM_IO_GPIO)
    target_compile_definitions(edgellm-e08-console PRIVATE EDGELLM_E08_LIVE=1)
endif()

edgellm_add_oracle_test(edgellm-e08-console
    PROJECT e08
    REPLAY replay/console_120.csv
    GRAMMAR grammar.gbnf
    PROMPT prompt.txt
    EXPECTED "${EDGELLM_ROOT}/tests/oracle/e08.expected.csv"
    ARGS "--daemon $<TARGET_FILE:edgellm-e08-daemon>")
