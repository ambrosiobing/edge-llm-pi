# projects/e12-stwin-band-reporter/CMakeLists.txt
# C++17, catalogue variant A: one process holds the 6 kHz frame path, the FFT, the rule
# and the engine. The engine is reached through common/cpp/edgellm.hpp, so the stub serves
# the same loop in CI and nothing here includes llama.h. The FFT is pocketfft (BSD
# 3-clause), one header, expected as the submodule third_party/pocketfft on its cpp branch:
#
#     git submodule add -b cpp https://github.com/mreineck/pocketfft third_party/pocketfft
#
# When the header is absent the build uses the radix-2 transform inside src/bands.cpp and
# says so at configure time, so a clone without the submodule still runs the oracle test
# and the tone test. The IO serial backend is compiled in with -DEDGELLM_IO_SERIAL=ON; the
# replay and frame-file paths need no backend.

find_package(Threads REQUIRED)

set(E12_POCKETFFT_DIR "${EDGELLM_ROOT}/third_party/pocketfft")
set(E12_POCKETFFT_HDR "${E12_POCKETFFT_DIR}/pocketfft_hdronly.h")

edgellm_add_project(edgellm-e12
    SOURCES src/main.cpp src/bands.cpp src/stwin_frames.cpp
    RULES src/rules_e12.c
    RULES_TEST src/rules_e12_test.c
    IO serial
    LIBS Threads::Threads
    NO_MAIN CXX_WRAPPER)
target_compile_features(edgellm-e12 PRIVATE cxx_std_17)

# The tone test: a synthetic sine through the window, the FFT and the band table, on
# whichever transform the configure step selected.
add_executable(e12_bands_test src/bands_test.cpp src/bands.cpp)
target_compile_features(e12_bands_test PRIVATE cxx_std_17)
target_compile_options(e12_bands_test PRIVATE -Wall -Wextra)
add_test(NAME e12_bands_test COMMAND e12_bands_test)

if(EXISTS "${E12_POCKETFFT_HDR}")
    foreach(t edgellm-e12 e12_bands_test)
        target_include_directories(${t} PRIVATE "${E12_POCKETFFT_DIR}")
        target_compile_definitions(${t} PRIVATE E12_HAVE_POCKETFFT=1)
    endforeach()
    message(STATUS "edgellm-e12: FFT is pocketfft from third_party/pocketfft")
else()
    message(STATUS "edgellm-e12: third_party/pocketfft is absent, the radix-2 transform in "
                   "src/bands.cpp is used; add the submodule for the bench build")
endif()

# The oracle: the band replay through the stub, compared with tests/oracle/e12.expected.csv
# on the rule and model columns. Exists only in a stub build.
edgellm_add_oracle_test(edgellm-e12
    PROJECT e12
    REPLAY replay/band_windows.csv
    GRAMMAR grammar.gbnf
    PROMPT prompt.txt
    EXPECTED "${EDGELLM_ROOT}/tests/oracle/e12.expected.csv")
