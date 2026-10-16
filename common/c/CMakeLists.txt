# edgellm_core: the decision core (real or stub), the io dispatcher with the replay and
# sysfs backends always present and the hardware backends by option, the CSV logger
# and the rule registry. Projects link it through edgellm_add_project.
set(EDGELLM_CORE_SOURCES
    src/io.c
    src/io_replay.c
    src/io_sysfs.c
    src/elog.c
    src/rules.c)
if(EDGELLM_STUB_MODEL)
    list(APPEND EDGELLM_CORE_SOURCES src/edgellm_stub.c)
else()
    list(APPEND EDGELLM_CORE_SOURCES src/edgellm.c)
endif()

add_library(edgellm_core STATIC ${EDGELLM_CORE_SOURCES})
target_include_directories(edgellm_core PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/include")
target_compile_options(edgellm_core PRIVATE -Wall -Wextra)
target_compile_definitions(edgellm_core PUBLIC EDGELLM_LLAMA_TAG="${LLAMA_CPP_TAG}")

if(EDGELLM_STUB_MODEL)
    target_compile_definitions(edgellm_core PUBLIC EDGELLM_STUB_MODEL=1)
else()
    target_link_libraries(edgellm_core PUBLIC llama)
endif()

if(EDGELLM_IO_GPIO)
    find_package(PkgConfig REQUIRED)
    pkg_check_modules(GPIOD REQUIRED IMPORTED_TARGET libgpiod>=2.0)
    target_sources(edgellm_core PRIVATE src/io_gpio.c)
    target_compile_definitions(edgellm_core PRIVATE EDGELLM_IO_GPIO=1)
    target_link_libraries(edgellm_core PUBLIC PkgConfig::GPIOD)
endif()
if(EDGELLM_IO_I2C)
    target_sources(edgellm_core PRIVATE src/io_i2c.c)
    target_compile_definitions(edgellm_core PRIVATE EDGELLM_IO_I2C=1)
endif()
if(EDGELLM_IO_IIO)
    target_sources(edgellm_core PRIVATE src/io_iio.c)
    target_compile_definitions(edgellm_core PRIVATE EDGELLM_IO_IIO=1)
endif()
if(EDGELLM_IO_SERIAL)
    target_sources(edgellm_core PRIVATE src/io_serial.c)
    target_compile_definitions(edgellm_core PRIVATE EDGELLM_IO_SERIAL=1)
endif()
if(EDGELLM_IO_CAN)
    target_sources(edgellm_core PRIVATE src/io_can.c)
    target_compile_definitions(edgellm_core PRIVATE EDGELLM_IO_CAN=1)
endif()

# The header-only C++ face. A C++ project links edgellm_cpp and includes edgellm.hpp.
add_library(edgellm_cpp INTERFACE)
target_include_directories(edgellm_cpp INTERFACE "${EDGELLM_ROOT}/common/cpp")
target_link_libraries(edgellm_cpp INTERFACE edgellm_core)
