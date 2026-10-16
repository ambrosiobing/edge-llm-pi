# projects/e07-iks4a1-narrator/CMakeLists.txt
# The narrator on its own main (C++17 over the C API through common/cpp/edgellm.hpp,
# catalogue variant B), its rule in C with six cases for the shared harness, and the
# oracle test over the synthetic day (stub builds only). The IIO backend is compiled
# in with -DEDGELLM_IO_IIO=ON for the bench build; the host build runs the replay
# without it and says so at configure time. edgellm_add_project and
# edgellm_add_oracle_test come from cmake/edgellm_project.cmake.

edgellm_add_project(edgellm-e07
    SOURCES src/main.cpp
    RULES src/rules_e07.c
    RULES_TEST src/rules_e07_test.c
    IO iio
    NO_MAIN
    CXX_WRAPPER)
target_compile_features(edgellm-e07 PRIVATE cxx_std_17)
# GCC 8 keeps std::filesystem in a separate archive; GCC 9 and later, and clang, do not.
if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU" AND CMAKE_CXX_COMPILER_VERSION VERSION_LESS 9)
    target_link_libraries(edgellm-e07 PRIVATE stdc++fs)
endif()

# The loop asks the model every 60 events by default, so the 1440-row day yields the
# 24 answers the oracle file expects; no extra argument is needed here.
edgellm_add_oracle_test(edgellm-e07
    PROJECT e07
    REPLAY replay/synthetic_day.csv
    GRAMMAR grammar.gbnf
    PROMPT prompt.txt
    EXPECTED "${EDGELLM_ROOT}/tests/oracle/e07.expected.csv")
