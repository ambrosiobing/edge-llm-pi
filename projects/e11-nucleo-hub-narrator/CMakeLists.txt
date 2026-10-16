# projects/e11-nucleo-hub-narrator/CMakeLists.txt
# The Nucleo CDC sensor hub narrator on the shared main: the frame layer that decodes the
# hub's CBOR frames (src/cbor_frames.c, tinycbor), the project's own main() that attaches
# it for a live run (src/e11_main.c, hence NO_MAIN), the rule and its six test cases for
# the shared harness, and the oracle test over the sixty-second replay (stub builds only).
#
# No IO backend is listed on purpose. common/c/src/io_serial.c is line-oriented and
# cannot carry a binary frame, so the frame layer opens the port itself with the same
# termios settings; the bench build needs no -DEDGELLM_IO_SERIAL=ON for this project.
#
# tinycbor (MIT) is found three ways, in this order:
#   1. pkg-config module "tinycbor" (tinycbor.pc; Debian trixie: libtinycbor-dev 0.6.1,
#      the .pc file in that package to verify on the first build);
#   2. a source checkout at tag v0.6.1 named by -DEDGELLM_E11_TINYCBOR_SRC=<path>, from
#      which the two decoder sources are compiled in: src/cborparser.c (the parser and
#      the integer accessors; the float accessors are inline in cbor.h at that tag) and
#      src/cborerrorstrings.c (cbor_error_string). Nothing of tinycbor is copied into
#      this repository;
#   3. neither: the e11 targets are skipped with a warning, so the other projects and the
#      shared tests still configure. CI installs libtinycbor-dev before the stub build.

find_package(Threads REQUIRED)

set(EDGELLM_E11_TINYCBOR_SRC "" CACHE PATH
    "A tinycbor checkout at v0.6.1; its two decoder sources are built in when none is installed")

set(E11_CBOR "")
find_package(PkgConfig QUIET)
if(PkgConfig_FOUND)
    pkg_check_modules(TINYCBOR QUIET IMPORTED_TARGET tinycbor)
endif()
if(TINYCBOR_FOUND)
    set(E11_CBOR PkgConfig::TINYCBOR)
    message(STATUS "edgellm-e11: tinycbor ${TINYCBOR_VERSION} through pkg-config")
elseif(EDGELLM_E11_TINYCBOR_SRC)
    foreach(f cborparser.c cborerrorstrings.c)
        if(NOT EXISTS "${EDGELLM_E11_TINYCBOR_SRC}/src/${f}")
            message(FATAL_ERROR "edgellm-e11: ${EDGELLM_E11_TINYCBOR_SRC}/src/${f} is missing; "
                                "point EDGELLM_E11_TINYCBOR_SRC at a tinycbor checkout at v0.6.1")
        endif()
    endforeach()
    add_library(e11_tinycbor STATIC
        "${EDGELLM_E11_TINYCBOR_SRC}/src/cborparser.c"
        "${EDGELLM_E11_TINYCBOR_SRC}/src/cborerrorstrings.c")
    target_include_directories(e11_tinycbor PUBLIC "${EDGELLM_E11_TINYCBOR_SRC}/src")
    set(E11_CBOR e11_tinycbor)
    message(STATUS "edgellm-e11: tinycbor decoder compiled from ${EDGELLM_E11_TINYCBOR_SRC}")
else()
    message(WARNING "edgellm-e11: tinycbor not found. Install libtinycbor-dev (pkg-config "
                    "module tinycbor) or pass -DEDGELLM_E11_TINYCBOR_SRC=<checkout at v0.6.1>; "
                    "the e11 targets, its rule cases and its oracle test are skipped.")
    return()
endif()

edgellm_add_project(edgellm-e11
    SOURCES src/e11_main.c src/cbor_frames.c
    RULES src/rules_e11.c
    RULES_TEST src/rules_e11_test.c
    LIBS ${E11_CBOR} Threads::Threads m
    NO_MAIN)

edgellm_add_oracle_test(edgellm-e11
    PROJECT e11
    REPLAY replay/hub_60s.csv
    GRAMMAR grammar.gbnf
    PROMPT prompt.txt
    EXPECTED "${EDGELLM_ROOT}/tests/oracle/e11.expected.csv"
    ARGS --n-predict 64)
