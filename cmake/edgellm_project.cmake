# edgellm_add_project(<target> SOURCES <c or cpp files...>
#                     [RULES <rules_eNN.c>] [RULES_TEST <rules_eNN_test.c>]
#                     [IO gpio i2c iio serial can ...] [LIBS <extra link items>]
#                     [NO_MAIN] [CXX_WRAPPER])
#
# Builds one project executable against the shared core. The target name is the
# installed binary name; the house convention is edgellm-eNN. SOURCES carry the
# project's own files, at least the one defining project_format_prompt. RULES names
# the file holding the rule_desc and its RULE_REGISTER line; it is compiled into the
# executable AND collected for tests/rules_test. RULES_TEST names the project's test
# cases for that harness. IO lists the live backends the bench run needs; a backend
# whose EDGELLM_IO_<NAME> option is OFF is reported at configure time, not at the
# bench. NO_MAIN leaves out common/c/src/edgellm_main.c for a project with its own
# main() (it may still call edgellm_main()). CXX_WRAPPER adds common/cpp to the
# include path.
#
#     edgellm_add_project(edgellm-e06
#         SOURCES src/main.c
#         RULES src/rules_e06.c
#         RULES_TEST src/rules_e06_test.c
#         IO i2c gpio)
#
# edgellm_add_oracle_test(<target> PROJECT eNN REPLAY <csv> GRAMMAR <gbnf> PROMPT <txt>
#                         EXPECTED <csv> [ARGS ...])
#
# Registers a ctest that runs the executable with --stub over the replay file and
# compares the log's rule and model columns against EXPECTED with tests/oracle_check.
# The test exists only in a stub build; with the real engine the same run needs a model
# file and is Joseph's bench job.

function(edgellm_add_project name)
    cmake_parse_arguments(P "NO_MAIN;CXX_WRAPPER" "" "SOURCES;RULES;RULES_TEST;IO;LIBS" ${ARGN})
    if(NOT P_SOURCES)
        message(FATAL_ERROR "edgellm_add_project(${name}): SOURCES is required")
    endif()
    add_executable(${name} ${P_SOURCES} ${P_RULES})
    if(NOT P_NO_MAIN)
        target_sources(${name} PRIVATE "${EDGELLM_ROOT}/common/c/src/edgellm_main.c")
    endif()
    target_link_libraries(${name} PRIVATE edgellm_core ${P_LIBS})
    target_compile_options(${name} PRIVATE -Wall -Wextra)
    if(P_CXX_WRAPPER)
        target_include_directories(${name} PRIVATE "${EDGELLM_ROOT}/common/cpp")
    endif()
    foreach(io IN LISTS P_IO)
        string(TOUPPER "${io}" IO)
        if(NOT EDGELLM_IO_${IO})
            message(STATUS "${name}: backend ${io} not compiled in; the bench build "
                           "needs -DEDGELLM_IO_${IO}=ON")
        endif()
    endforeach()
    foreach(f IN LISTS P_RULES)
        get_filename_component(abs "${f}" ABSOLUTE)
        set_property(GLOBAL APPEND PROPERTY EDGELLM_RULE_SOURCES "${abs}")
    endforeach()
    foreach(f IN LISTS P_RULES_TEST)
        get_filename_component(abs "${f}" ABSOLUTE)
        set_property(GLOBAL APPEND PROPERTY EDGELLM_RULE_TEST_SOURCES "${abs}")
    endforeach()
    set_property(GLOBAL APPEND PROPERTY EDGELLM_PROJECTS "${name}")
endfunction()

function(edgellm_add_oracle_test name)
    cmake_parse_arguments(T "" "PROJECT;REPLAY;GRAMMAR;PROMPT;EXPECTED" "ARGS" ${ARGN})
    foreach(req PROJECT REPLAY GRAMMAR PROMPT EXPECTED)
        if(NOT T_${req})
            message(FATAL_ERROR "edgellm_add_oracle_test(${name}): ${req} is required")
        endif()
    endforeach()
    if(NOT EDGELLM_STUB_MODEL)
        message(STATUS "${name}: oracle test skipped, needs -DEDGELLM_STUB_MODEL=ON")
        return()
    endif()
    foreach(v REPLAY GRAMMAR PROMPT EXPECTED)
        get_filename_component(T_${v} "${T_${v}}" ABSOLUTE)
    endforeach()
    # ARGS is a CMake list, so "-DEXTRA=${T_ARGS}" would hand run_oracle.cmake the string
    # "--n-predict;64;--temperature;0.3", and the separate_arguments() there splits on
    # whitespace, not on semicolons: the whole thing arrived as ONE argument. Every project
    # that uses the shared main then printed its usage and exited 2, which is what e03,
    # e16, e17 and e19 did the first time these tests were ever registered; e08 and e09
    # have their own main() and silently ignored the token instead, which is worse.
    # Joining with spaces here is what separate_arguments expects. The constraint it buys:
    # no single ARGS entry may contain a space. None does, and they are flags and numbers.
    # Found Friday 16 October 2026.
    string(JOIN " " T_ARGS_LINE ${T_ARGS})
    foreach(a IN LISTS T_ARGS)
        if(a MATCHES " ")
            message(FATAL_ERROR
                "edgellm_add_oracle_test(${name}): the ARGS entry '${a}' contains a space, "
                "which the oracle runner cannot carry; pass it another way")
        endif()
    endforeach()
    add_test(NAME ${name}-oracle
             COMMAND ${CMAKE_COMMAND}
                     -DBIN=$<TARGET_FILE:${name}>
                     -DCHECK=$<TARGET_FILE:oracle_check>
                     -DPROJECT=${T_PROJECT}
                     -DREPLAY=${T_REPLAY}
                     -DGRAMMAR=${T_GRAMMAR}
                     -DPROMPT=${T_PROMPT}
                     -DEXPECTED=${T_EXPECTED}
                     -DLOG=${CMAKE_CURRENT_BINARY_DIR}/${name}.log.csv
                     "-DEXTRA=${T_ARGS_LINE}"
                     -P "${EDGELLM_ROOT}/cmake/run_oracle.cmake")
endfunction()
