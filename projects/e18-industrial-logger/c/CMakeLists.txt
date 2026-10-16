# projects/e18-industrial-logger/c/CMakeLists.txt
#
# The C half of project e18: the shared object libedgellm_abi.so that carries the
# four declarations of abi.h, the daemon that serves them over a Unix domain socket,
# and the test that checks the ABI and measures what the boundary costs per call.
#
# This is a project in its own right, not a subdirectory of the repository's top
# level CMakeLists.txt, and that is the point of the chapter: the inference daemon
# has its own lifetime and is configured, built, versioned and installed without the
# device service being rebuilt. The repository's top level file globs
# projects/*/CMakeLists.txt, one level deep, so this file is never picked up by it
# and is always configured on its own:
#
#     cmake -S projects/e18-industrial-logger/c -B build-e18c -DEDGELLM_STUB_MODEL=ON
#     cmake --build build-e18c
#     ctest --test-dir build-e18c --output-on-failure
#
# With the real engine, libllama must already be built or installed, because this
# project does not add llama.cpp as a subdirectory: the bench passes
# -DEDGELLM_LLAMA_LIBRARY and -DEDGELLM_LLAMA_INCLUDE_DIR, or leaves them unset and
# lets find_library and find_path locate an installed one, which is what the Yocto
# recipe does with the llama-cpp recipe's sysroot.
cmake_minimum_required(VERSION 3.16)
project(edgellm_e18_abi LANGUAGES C VERSION 0.1.0)

option(EDGELLM_STUB_MODEL "Serve edgellm.h from the grammar-only stub, no llama.cpp" OFF)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()

get_filename_component(EDGELLM_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/../../.." ABSOLUTE)
if(NOT EXISTS "${EDGELLM_ROOT}/common/c/include/edgellm.h")
    message(FATAL_ERROR "common/c not found above ${CMAKE_CURRENT_SOURCE_DIR}; this "
                        "project is configured from inside its own checkout")
endif()
include("${EDGELLM_ROOT}/cmake/llama_tag.cmake")

# One source of the decision core, chosen the same way common/c/CMakeLists.txt
# chooses it, so the daemon and the C projects answer identically.
if(EDGELLM_STUB_MODEL)
    set(E18_CORE_ENGINE "${EDGELLM_ROOT}/common/c/src/edgellm_stub.c")
else()
    set(E18_CORE_ENGINE "${EDGELLM_ROOT}/common/c/src/edgellm.c")
endif()

add_library(edgellm_abi SHARED abi.c "${E18_CORE_ENGINE}")
target_include_directories(edgellm_abi
    PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}"
    PRIVATE "${EDGELLM_ROOT}/common/c/include")
target_compile_options(edgellm_abi PRIVATE -Wall -Wextra)
target_compile_definitions(edgellm_abi PRIVATE EDGELLM_LLAMA_TAG="${LLAMA_CPP_TAG}")
find_library(E18_M m)
if(E18_M)
    target_link_libraries(edgellm_abi PRIVATE ${E18_M})
endif()
find_package(Threads REQUIRED)
target_link_libraries(edgellm_abi PRIVATE Threads::Threads)

# The shared object's SOVERSION is the ABI's major version read out of abi.h, so a
# major bump renames the file on disk and a caller linked against the old major
# keeps the old file rather than loading an incompatible one. A minor bump does not
# rename it, which is what a minor bump means.
file(STRINGS abi.h E18_ABI_VERSION_LINE REGEX "^#define EDGELLM_ABI_VERSION ")
string(REGEX MATCH "[0-9]+" E18_ABI_VERSION "${E18_ABI_VERSION_LINE}")
math(EXPR E18_ABI_MAJOR "${E18_ABI_VERSION} / 1000")
math(EXPR E18_ABI_MINOR "${E18_ABI_VERSION} % 1000")
set_target_properties(edgellm_abi PROPERTIES
    VERSION "${E18_ABI_MAJOR}.${E18_ABI_MINOR}.0"
    SOVERSION "${E18_ABI_MAJOR}")
message(STATUS "edgellm_abi: ABI version ${E18_ABI_VERSION} "
               "(major ${E18_ABI_MAJOR}, minor ${E18_ABI_MINOR})")

if(NOT EDGELLM_STUB_MODEL)
    find_path(EDGELLM_LLAMA_INCLUDE_DIR llama.h REQUIRED)
    find_library(EDGELLM_LLAMA_LIBRARY llama REQUIRED)
    target_include_directories(edgellm_abi PRIVATE "${EDGELLM_LLAMA_INCLUDE_DIR}")
    target_link_libraries(edgellm_abi PRIVATE "${EDGELLM_LLAMA_LIBRARY}")
else()
    target_compile_definitions(edgellm_abi PRIVATE EDGELLM_STUB_MODEL=1)
endif()

add_executable(edgellm-e18-daemon daemon.c)
target_compile_options(edgellm-e18-daemon PRIVATE -Wall -Wextra)
target_link_libraries(edgellm-e18-daemon PRIVATE edgellm_abi)

add_executable(e18_abi_test abi_test.c)
target_include_directories(e18_abi_test PRIVATE "${EDGELLM_ROOT}/common/c/include")
target_compile_options(e18_abi_test PRIVATE -Wall -Wextra)
target_link_libraries(e18_abi_test PRIVATE edgellm_abi)
if(E18_M)
    target_link_libraries(e18_abi_test PRIVATE ${E18_M})
endif()

# The test runs with no model file only in a stub build; with the real engine it
# needs --model and is a bench run, so it is registered without one and the real
# figure is taken by hand on the Pi 4B (project.toml, [measure]).
enable_testing()
if(EDGELLM_STUB_MODEL)
    add_test(NAME edgellm-e18-abi
             COMMAND e18_abi_test
                     --grammar "${CMAKE_CURRENT_SOURCE_DIR}/../grammar.gbnf"
                     --prompt "${CMAKE_CURRENT_SOURCE_DIR}/../prompt.txt"
                     --calls 200)
else()
    message(STATUS "e18_abi_test: not registered with ctest, the real engine needs "
                   "--model; run it by hand on the board")
endif()

include(GNUInstallDirs)
install(TARGETS edgellm_abi edgellm-e18-daemon
        LIBRARY DESTINATION "${CMAKE_INSTALL_LIBDIR}"
        RUNTIME DESTINATION "${CMAKE_INSTALL_BINDIR}")
install(FILES abi.h DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}/edgellm-e18")
