# projects/e17-cellular-summariser/CMakeLists.txt
# The cellular status summariser: the shared loop over one status sample per event, the
# rule and its six cases for the shared harness tests/rules_test, the AT parser with
# its own eleven case test, and the oracle test over the modelled day (stub builds
# only). edgellm_add_project and edgellm_add_oracle_test come from
# cmake/edgellm_project.cmake.
#
# NO_MAIN plus the shared main listed as a source, with EDGELLM_NO_MAIN defined: the
# project's own main() in src/e17_main.c dispatches on --modem, which runs the sampler
# half (the AT parser over the serial backend, writing the sample CSV the loop then
# replays), and otherwise calls edgellm_main() with argv untouched. The loop itself is
# not modified.
#
# IO serial: the sampler reads the modem's replies through common/c/src/io_serial.c, so
# the bench build needs -DEDGELLM_IO_SERIAL=ON. A build without it still compiles and
# still runs the loop over a replay file; only --modem is refused, with the backend's
# own message, which is what CI does.

edgellm_add_project(edgellm-e17
    SOURCES src/e17_main.c src/at_parser.c "${EDGELLM_ROOT}/common/c/src/edgellm_main.c"
    RULES src/rules_e17.c
    RULES_TEST src/rules_e17_test.c
    IO serial
    NO_MAIN)
target_compile_definitions(edgellm-e17 PRIVATE EDGELLM_NO_MAIN=1)

edgellm_add_oracle_test(edgellm-e17
    PROJECT e17
    REPLAY replay/day_1440.csv
    GRAMMAR grammar.gbnf
    PROMPT prompt.txt
    EXPECTED "${EDGELLM_ROOT}/tests/oracle/e17.expected.csv"
    ARGS --n-predict 64 --temperature 0.3)

# The parser's own test links nothing but the parser: no engine, no io, no model, so it
# runs everywhere the repository builds and it is the test that has to pass before a
# summary of a modem's state means anything.
if(EDGELLM_BUILD_TESTS)
    add_executable(e17_at_parser_test src/at_parser_test.c src/at_parser.c)
    target_include_directories(e17_at_parser_test PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/src")
    target_compile_options(e17_at_parser_test PRIVATE -Wall -Wextra)
    add_test(NAME e17-at-parser COMMAND e17_at_parser_test)
endif()
