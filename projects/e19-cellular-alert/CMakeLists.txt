# projects/e19-cellular-alert/CMakeLists.txt
# The event driven cellular alert composer: the project's own loop and send path, the
# field checker, the AT reply parser, the rule and its six classification cases for the
# shared harness, this project's own test binary for the field checker and the send
# gate, and the oracle test over the fifty replayed events (stub builds only).
#
# NO_MAIN without the shared main listed as a source: src/e19_main.c carries the one
# main() of this project, because the thing that may be sent is the model's answer and
# the shared loop in common/c/src/edgellm_main.c hands no hook the model's answer. It
# takes the same options and reuses io.h, rules.h, elog.h and edgellm.h unchanged; see
# the file's own header for the argument.
#
# The two live backends: iio for the ADXL345 through the driver of the Linux volume's
# project 5, and serial so that a bench build carries the termios backend the other
# cellular projects use. The modem's AT port is opened by this project directly, since
# the serial backend parses replies and cannot write a command.
#
# src/at_parser.c belongs to project e17, the cellular status summariser, which did not
# exist when this project was written. When e17 is written the file moves to
# projects/e17-cellular-summariser/src/at_parser.c and the SOURCES line below lists it
# there as ../e17-cellular-summariser/src/at_parser.c, by path and with no copy.
#
# libm is linked for the one square root in src/e19_main.c, the peak magnitude in g.
# src/rules_e19.c needs none: it is compiled a second time into tests/rules_test, whose
# link line is edgellm_core alone, so a rule that pulled in a library would stop that
# harness for every project at once.

edgellm_add_project(edgellm-e19
    SOURCES src/e19_main.c src/compose_check.c src/at_parser.c
    RULES src/rules_e19.c
    RULES_TEST src/rules_e19_test.c
    IO serial iio
    LIBS m
    NO_MAIN)

edgellm_add_oracle_test(edgellm-e19
    PROJECT e19
    REPLAY replay/alerts_50.csv
    GRAMMAR grammar.gbnf
    PROMPT prompt.txt
    EXPECTED "${EDGELLM_ROOT}/tests/oracle/e19.expected.csv"
    ARGS --n-predict 64 --temperature 0.3)

# The field checker and the send gate, as one binary with its own exit status: the
# shared harness compares decision indices and has nowhere to put a case about a
# message or about time. src/compose_check.c and src/rules_e19.c are compiled into it
# so that the cases run against the same code the loop runs.
if(EDGELLM_BUILD_TESTS)
    add_executable(e19_compose_check_test
        src/compose_check_test.c src/compose_check.c src/rules_e19.c)
    target_include_directories(e19_compose_check_test PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}/src")
    target_link_libraries(e19_compose_check_test PRIVATE edgellm_core)
    target_compile_options(e19_compose_check_test PRIVATE -Wall -Wextra)
    add_test(NAME e19_compose_check COMMAND e19_compose_check_test)
endif()
