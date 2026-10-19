# Host tests. Three things run under ctest:
#   oracle_check      the C comparer every project's oracle test uses
#   rules_test        every registered rule on fixed events, cases from rules_eNN_test.c
#   edgellm-smoke     project e00, the stub loop over tests/data/smoke.csv, checked
#                     against tests/data/smoke.expected.csv
# The per-project oracle files live in tests/oracle/eNN.expected.csv and are written by
# the project authors; each project registers its own test with edgellm_add_oracle_test.

add_executable(oracle_check oracle_check.c)
target_compile_options(oracle_check PRIVATE -Wall -Wextra)
add_test(NAME oracle_check-self
         COMMAND oracle_check "${CMAKE_CURRENT_SOURCE_DIR}/data/smoke.expected.csv"
                              "${CMAKE_CURRENT_SOURCE_DIR}/data/smoke.expected.csv")

# The smoke project is a project like any other, so it also contributes its rule and
# its cases to rules_test.
edgellm_add_project(edgellm-smoke
    SOURCES smoke/smoke_project.c
    RULES smoke/rules_e00.c
    RULES_TEST smoke/rules_e00_test.c)
edgellm_add_oracle_test(edgellm-smoke
    PROJECT e00
    REPLAY data/smoke.csv
    GRAMMAR data/smoke.gbnf
    PROMPT data/smoke_prompt.txt
    EXPECTED data/smoke.expected.csv)

get_property(RULE_SOURCES GLOBAL PROPERTY EDGELLM_RULE_SOURCES)
get_property(RULE_TEST_SOURCES GLOBAL PROPERTY EDGELLM_RULE_TEST_SOURCES)
add_executable(rules_test rules_test.c ${RULE_SOURCES} ${RULE_TEST_SOURCES})
target_include_directories(rules_test PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}")
target_link_libraries(rules_test PRIVATE edgellm_core)
target_compile_options(rules_test PRIVATE -Wall -Wextra)
add_test(NAME rules_test COMMAND rules_test)

# elog's refusal to overwrite a log. Registered in every build and not only the stub one,
# because it loads no model and makes no decision: it opens files and reads their size.
# The scratch directory is the build tree, so the test writes nothing into the source.
add_executable(elog_guard_test elog_guard_test.c)
target_link_libraries(elog_guard_test PRIVATE edgellm_core)
target_compile_options(elog_guard_test PRIVATE -Wall -Wextra)
add_test(NAME elog_guard_test COMMAND elog_guard_test "${CMAKE_CURRENT_BINARY_DIR}")

# The C++ wrapper compiles in every build; its run needs the stub.
add_executable(cpp_wrapper_test cpp_wrapper_test.cpp)
target_link_libraries(cpp_wrapper_test PRIVATE edgellm_cpp)
target_compile_options(cpp_wrapper_test PRIVATE -Wall -Wextra)
if(EDGELLM_STUB_MODEL)
    add_test(NAME cpp_wrapper_test
             COMMAND cpp_wrapper_test "${CMAKE_CURRENT_SOURCE_DIR}/data/smoke.gbnf"
                                      "${CMAKE_CURRENT_SOURCE_DIR}/data/smoke_prompt.txt")
endif()
