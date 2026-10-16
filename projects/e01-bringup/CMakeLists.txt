# projects/e01-bringup/CMakeLists.txt
# The C++17 harness that is priced against llama-bench. It needs the real engine, so the
# stub CI build leaves it out; the target name llama comes from the top-level
# CMakeLists.txt, which adds the llama.cpp submodule at the pinned tag.

if(NOT EDGELLM_STUB_MODEL)
  add_executable(e01_bench_harness src/bench_harness.cpp)
  target_compile_features(e01_bench_harness PRIVATE cxx_std_17)
  target_link_libraries(e01_bench_harness PRIVATE llama)
endif()
