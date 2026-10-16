# projects/e04-thermal-narrator/CMakeLists.txt
# The HTTP client of llama-server (catalogue variant D): built against libcurl and the
# shared io, rules and elog sources of common/c, never against the engine, so the same
# binary serves the stub CI build, the bench build and the Yocto build unchanged. The
# rule and its six cases join the shared harness tests/rules_test through the two
# global properties cmake/edgellm_project.cmake defines. The oracle test runs the
# client over the replay file against tools/mock_server.py, which answers every
# request with the grammar's first expansion, and compares the log with
# tests/oracle/e04.expected.csv through tests/oracle_check. libcurl is the curl
# licence; on Debian and Ubuntu the package is libcurl4-openssl-dev.

find_package(CURL)
find_package(Python3 COMPONENTS Interpreter)

if(NOT CURL_FOUND)
    message(STATUS "edgellm-e04: libcurl not found (libcurl4-openssl-dev); "
                   "the client is not built")
    return()
endif()

set(E04_SHARED
    "${EDGELLM_ROOT}/common/c/src/io.c"
    "${EDGELLM_ROOT}/common/c/src/io_replay.c"
    "${EDGELLM_ROOT}/common/c/src/io_sysfs.c"
    "${EDGELLM_ROOT}/common/c/src/elog.c"
    "${EDGELLM_ROOT}/common/c/src/rules.c")

add_executable(edgellm-e04 src/e04_client.c src/rules_e04.c ${E04_SHARED})
target_include_directories(edgellm-e04 PRIVATE "${EDGELLM_ROOT}/common/c/include")
target_compile_options(edgellm-e04 PRIVATE -Wall -Wextra)
target_link_libraries(edgellm-e04 PRIVATE CURL::libcurl)

get_filename_component(E04_RULE src/rules_e04.c ABSOLUTE)
get_filename_component(E04_RULE_TEST src/rules_e04_test.c ABSOLUTE)
set_property(GLOBAL APPEND PROPERTY EDGELLM_RULE_SOURCES "${E04_RULE}")
set_property(GLOBAL APPEND PROPERTY EDGELLM_RULE_TEST_SOURCES "${E04_RULE_TEST}")
set_property(GLOBAL APPEND PROPERTY EDGELLM_PROJECTS edgellm-e04)

if(EDGELLM_BUILD_TESTS AND Python3_Interpreter_FOUND)
    add_test(NAME edgellm-e04-oracle
        COMMAND ${Python3_EXECUTABLE} "${CMAKE_CURRENT_SOURCE_DIR}/tools/mock_server.py"
            --run
            --check $<TARGET_FILE:oracle_check>
                    "${EDGELLM_ROOT}/tests/oracle/e04.expected.csv"
                    "${CMAKE_CURRENT_BINARY_DIR}/edgellm-e04.log.csv"
            --
            $<TARGET_FILE:edgellm-e04> --project e04
                --replay "${CMAKE_CURRENT_SOURCE_DIR}/replay/benchmark_180.csv"
                --grammar "${CMAKE_CURRENT_SOURCE_DIR}/grammar.gbnf"
                --prompt "${CMAKE_CURRENT_SOURCE_DIR}/prompt.txt"
                --server "http://127.0.0.1:{PORT}"
                --log "${CMAKE_CURRENT_BINARY_DIR}/edgellm-e04.log.csv"
                --rtt "${CMAKE_CURRENT_BINARY_DIR}/edgellm-e04.rtt.csv")
elseif(EDGELLM_BUILD_TESTS)
    message(STATUS "edgellm-e04: no Python interpreter, the oracle test is not registered")
endif()
