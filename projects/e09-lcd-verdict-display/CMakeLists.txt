# projects/e09-lcd-verdict-display/CMakeLists.txt
# The LCD prompt and verdict display: a C++17 executable with its own main() on the
# shared core, linked to the engine in one process through common/cpp/edgellm.hpp, to
# libdrm for the panel, and to the shared io, rules and elog objects of edgellm_core.
# The rule and its six cases join the shared harness; the oracle test runs the stub
# build over the replay with --headless, so CI needs neither a panel nor a model file.
#
# libdrm is found through pkg-config. Where it is absent the target still builds and
# refuses anything but --headless at run time, so a runner without libdrm-dev keeps
# the oracle test; the bench build and the Yocto recipe carry the library. The bench
# run also needs the iio and gpio backends: configure with -DEDGELLM_IO_IIO=ON
# -DEDGELLM_IO_GPIO=ON, which edgellm_add_project reports when they are off.

find_package(PkgConfig QUIET)
if(PkgConfig_FOUND)
  pkg_check_modules(DRM QUIET IMPORTED_TARGET libdrm)
endif()

edgellm_add_project(edgellm-e09
    SOURCES src/main.cpp src/panel.cpp src/touch.cpp
    RULES src/rules_e09.c
    RULES_TEST src/rules_e09_test.c
    IO iio gpio
    NO_MAIN
    CXX_WRAPPER)
target_compile_features(edgellm-e09 PRIVATE cxx_std_17)

if(DRM_FOUND)
  target_link_libraries(edgellm-e09 PRIVATE PkgConfig::DRM)
  target_compile_definitions(edgellm-e09 PRIVATE EDGELLM_HAVE_DRM=1)
else()
  message(STATUS "edgellm-e09: libdrm not found (apt install libdrm-dev); the panel is "
                 "compiled out and only --headless runs")
endif()

edgellm_add_oracle_test(edgellm-e09
    PROJECT e09
    REPLAY replay/adxl345_200.csv
    GRAMMAR grammar.gbnf
    PROMPT prompt.txt
    EXPECTED "${EDGELLM_ROOT}/tests/oracle/e09.expected.csv"
    ARGS --headless)
