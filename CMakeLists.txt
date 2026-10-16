# edge-llm-pi: the shared C core, the C++ wrapper, every C and C++ project, and the
# host-side tests. Rust projects build with cargo and Python projects with pip from
# their own manifests; CI runs all three by manifest.
#
# Host, with no model file (what CI does):
#     cmake -S . -B build -DEDGELLM_STUB_MODEL=ON
#     cmake --build build
#     ctest --test-dir build --output-on-failure
#
# Bench, with the real engine from the submodule and every I/O backend:
#     cmake -S . -B build -DEDGELLM_IO_GPIO=ON -DEDGELLM_IO_I2C=ON -DEDGELLM_IO_IIO=ON \
#           -DEDGELLM_IO_SERIAL=ON -DEDGELLM_IO_CAN=ON
#
# Yocto (meta-edgellm/recipes-app/edgellm-common.inc) adds -DEDGELLM_SYSTEM_LLAMA=ON so
# the installed libllama from the llama-cpp recipe is linked instead of the submodule.
cmake_minimum_required(VERSION 3.16)
project(edgellm LANGUAGES C CXX)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()

option(EDGELLM_STUB_MODEL "Serve edgellm.h from the grammar-only stub, no llama.cpp" OFF)
option(EDGELLM_SYSTEM_LLAMA "Link an installed libllama instead of the submodule" OFF)
option(EDGELLM_BUILD_LLAMA_TOOLS "Build llama-bench and llama-server from the submodule" ON)
option(EDGELLM_BUILD_PROJECTS "Add every projects/*/CMakeLists.txt" ON)
option(EDGELLM_BUILD_TESTS "Build the host tests and register them with ctest" ON)
option(EDGELLM_IO_GPIO "Compile the libgpiod 2 backend (needs libgpiod-dev)" OFF)
option(EDGELLM_IO_I2C "Compile the i2c-dev backend" OFF)
option(EDGELLM_IO_IIO "Compile the IIO sysfs backend" OFF)
option(EDGELLM_IO_SERIAL "Compile the termios serial backend" OFF)
option(EDGELLM_IO_CAN "Compile the SocketCAN backend" OFF)

set(EDGELLM_ROOT "${CMAKE_CURRENT_SOURCE_DIR}")
include(cmake/llama_tag.cmake)
include(cmake/edgellm_project.cmake)

if(NOT EDGELLM_STUB_MODEL)
    if(EDGELLM_SYSTEM_LLAMA)
        find_path(LLAMA_INCLUDE_DIR llama.h REQUIRED)
        find_library(LLAMA_LIBRARY llama REQUIRED)
        add_library(llama INTERFACE IMPORTED)
        set_target_properties(llama PROPERTIES
            INTERFACE_INCLUDE_DIRECTORIES "${LLAMA_INCLUDE_DIR}"
            INTERFACE_LINK_LIBRARIES "${LLAMA_LIBRARY}")
    else()
        set(LLAMA_DIR "${EDGELLM_ROOT}/third_party/llama.cpp")
        if(NOT EXISTS "${LLAMA_DIR}/CMakeLists.txt")
            message(FATAL_ERROR
                "third_party/llama.cpp is empty. Run\n"
                "    git submodule update --init --recursive\n"
                "and check that it sits at tag ${LLAMA_CPP_TAG} (cmake/llama_tag.cmake), "
                "or configure with -DEDGELLM_STUB_MODEL=ON for a model-free build.")
        endif()
        set(LLAMA_BUILD_TESTS OFF CACHE BOOL "" FORCE)
        set(LLAMA_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
        set(LLAMA_BUILD_COMMON ON CACHE BOOL "" FORCE)
        set(LLAMA_BUILD_TOOLS ${EDGELLM_BUILD_LLAMA_TOOLS} CACHE BOOL "" FORCE)
        set(LLAMA_BUILD_SERVER ${EDGELLM_BUILD_LLAMA_TOOLS} CACHE BOOL "" FORCE)
        set(LLAMA_CURL OFF CACHE BOOL "" FORCE)
        set(LLAMA_OPENSSL OFF CACHE BOOL "" FORCE)
        add_subdirectory("${LLAMA_DIR}" "${CMAKE_BINARY_DIR}/llama.cpp")
    endif()
endif()

# enable_testing() BEFORE any add_subdirectory, and this order is load bearing. CMake
# writes a CTestTestfile.cmake for a directory only if testing was enabled when that
# directory was processed, so an add_test() in a subdirectory added earlier is dropped
# without a word. This file used to call enable_testing() further down, just before
# add_subdirectory(tests), and the effect was that twelve per-project oracle tests and
# five unit tests registered in projects/*/CMakeLists.txt never existed: ctest reported
# "100% tests passed, 0 tests failed out of 4" and the four were the ones in tests/.
# A green run that silently omits most of the suite is worse than a red one. Found on
# Friday 16 October 2026 on the first build in skyhorizon's WSL, by noticing that four
# is too few for twenty projects.
if(EDGELLM_BUILD_TESTS)
    enable_testing()
endif()

add_subdirectory(common/c)

if(EDGELLM_BUILD_PROJECTS)
    file(GLOB EDGELLM_PROJECT_CMAKE LIST_DIRECTORIES false
         "${EDGELLM_ROOT}/projects/*/CMakeLists.txt")
    foreach(cm IN LISTS EDGELLM_PROJECT_CMAKE)
        get_filename_component(dir "${cm}" DIRECTORY)
        add_subdirectory("${dir}")
    endforeach()
endif()

if(EDGELLM_BUILD_TESTS AND EXISTS "${EDGELLM_ROOT}/tests/CMakeLists.txt")
    add_subdirectory(tests)
endif()
