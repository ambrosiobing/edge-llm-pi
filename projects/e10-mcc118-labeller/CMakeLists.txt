# projects/e10-mcc118-labeller/CMakeLists.txt
# The MCC 118 voltage event labeller on the shared loop. The project owns main()
# (src/e10_main.c) because its live source is project-local: io.h has no hook for a
# backend outside common/c/src, so main() forks src/io_mcc118.c and feeds the shared
# loop through a pipe in the replay shape. The shared main is compiled in with
# EDGELLM_NO_MAIN so that edgellm_main() exists without its main().
#
# EDGELLM_DAQHATS links libdaqhats (MIT, built from the mccdaq/daqhats repository by its
# install.sh: /usr/local/include/daqhats/daqhats.h and /usr/local/lib/libdaqhats.so)
# and compiles the live source in. It is OFF by default, so the host and the CI stub
# build need no HAT and no library; the bench build on the Pi 4B sets it ON. A binary
# without it refuses --live mcc118 with the reason.
#
# edgellm_add_project and edgellm_add_oracle_test come from cmake/edgellm_project.cmake.

option(EDGELLM_DAQHATS "e10: link libdaqhats and compile the live MCC 118 source" OFF)

edgellm_add_project(edgellm-e10
    SOURCES src/e10_main.c "${EDGELLM_ROOT}/common/c/src/edgellm_main.c"
    RULES src/rules_e10.c
    RULES_TEST src/rules_e10_test.c
    NO_MAIN)
target_compile_definitions(edgellm-e10 PRIVATE EDGELLM_NO_MAIN=1)

if(EDGELLM_DAQHATS)
    find_path(DAQHATS_INCLUDE_DIR daqhats/daqhats.h HINTS /usr/local/include)
    find_library(DAQHATS_LIBRARY daqhats HINTS /usr/local/lib)
    if(NOT DAQHATS_INCLUDE_DIR OR NOT DAQHATS_LIBRARY)
        message(FATAL_ERROR "edgellm-e10: EDGELLM_DAQHATS is ON but daqhats/daqhats.h or "
                            "libdaqhats was not found; run the daqhats repository's "
                            "install.sh on the board first (see wiring.md)")
    endif()
    target_sources(edgellm-e10 PRIVATE src/io_mcc118.c)
    target_include_directories(edgellm-e10 PRIVATE "${DAQHATS_INCLUDE_DIR}")
    target_link_libraries(edgellm-e10 PRIVATE "${DAQHATS_LIBRARY}")
    target_compile_definitions(edgellm-e10 PRIVATE EDGELLM_DAQHATS=1)
else()
    message(STATUS "edgellm-e10: the live MCC 118 source is not compiled in; the bench "
                   "build needs -DEDGELLM_DAQHATS=ON")
endif()

edgellm_add_oracle_test(edgellm-e10
    PROJECT e10
    REPLAY replay/stimuli_300.csv
    GRAMMAR grammar.gbnf
    PROMPT prompt.txt
    EXPECTED "${EDGELLM_ROOT}/tests/oracle/e10.expected.csv")
