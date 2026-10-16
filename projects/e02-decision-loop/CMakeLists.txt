# projects/e02-decision-loop/CMakeLists.txt
# The decision loop on the shared main, its rule and the rule's six test cases for the
# shared harness, the oracle test over the synthetic replay (stub builds only), and the
# two comparison loops, variant A (C++17 on llama.h directly) and variant B (C++17 over
# the C API through common/cpp/edgellm.hpp), which need the real engine and are left
# out of the stub CI build. edgellm_add_project and edgellm_add_oracle_test come from
# cmake/edgellm_project.cmake; edgellm_core, edgellm_cpp and llama are the targets the
# top-level CMakeLists.txt defines.

edgellm_add_project(edgellm-e02
    SOURCES src/e02_main.c
    RULES src/rules_e02.c
    RULES_TEST src/rules_e02_test.c)

edgellm_add_oracle_test(edgellm-e02
    PROJECT e02
    REPLAY replay/synthetic_1000.csv
    GRAMMAR grammar.gbnf
    PROMPT prompt.txt
    EXPECTED "${EDGELLM_ROOT}/tests/oracle/e02.expected.csv")

if(NOT EDGELLM_STUB_MODEL)
  add_executable(e02_variant_a src/variant_a_cpp.cpp src/rules_e02.c)
  target_compile_features(e02_variant_a PRIVATE cxx_std_17)
  target_compile_options(e02_variant_a PRIVATE -Wall -Wextra)
  target_link_libraries(e02_variant_a PRIVATE llama edgellm_core)

  add_executable(e02_variant_b src/variant_b_capi.cpp src/rules_e02.c)
  target_compile_features(e02_variant_b PRIVATE cxx_std_17)
  target_compile_options(e02_variant_b PRIVATE -Wall -Wextra)
  target_link_libraries(e02_variant_b PRIVATE edgellm_cpp)
endif()
