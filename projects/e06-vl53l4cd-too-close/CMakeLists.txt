# projects/e06-vl53l4cd-too-close/CMakeLists.txt
# The decision loop on the shared main with the project's own main() in front of it, its
# rule and the rule's six cases for the shared harness, the oracle test over the replay of
# approaches (stub builds only), and, when the ST ULD is pointed at, the live sampler over
# i2c-dev. The ULD (STSW-IMG026) is not in this tree: its download sits behind the licence
# page on st.com, so it is unpacked out of tree and named here. Without it the binary builds
# the replay path only and refuses --live with the reason. edgellm_add_project and
# edgellm_add_oracle_test come from cmake/edgellm_project.cmake.

set(EDGELLM_VL53L4CD_ULD_DIR "" CACHE PATH
    "Folder of the unpacked ST VL53L4CD ULD: api and calibration sources, headers, platform.h")
set(EDGELLM_VL53L4CD_WAIT_NAME "VL53L4CD_WaitMs" CACHE STRING
    "The ULD's millisecond wait as platform.h names it: VL53L4CD_WaitMs, or WaitMs (2021 mirror)")

set(E06_SOURCES src/e06_main.c)
set(E06_HAVE_ULD OFF)
if(EDGELLM_VL53L4CD_ULD_DIR)
    # Both spellings seen on Wednesday 14 October 2026: VL53L4CD_api.c in the standalone
    # ULD's folder name, vl53l4cd_api.c in the X-CUBE port; the glob takes either.
    file(GLOB E06_ULD_API "${EDGELLM_VL53L4CD_ULD_DIR}/[Vv][Ll]53[Ll]4[Cc][Dd]_api.c")
    file(GLOB E06_ULD_CAL "${EDGELLM_VL53L4CD_ULD_DIR}/[Vv][Ll]53[Ll]4[Cc][Dd]_calibration.c")
    file(GLOB E06_ULD_HDR "${EDGELLM_VL53L4CD_ULD_DIR}/[Vv][Ll]53[Ll]4[Cc][Dd]_api.h")
    if(NOT E06_ULD_API OR NOT E06_ULD_HDR OR NOT EXISTS "${EDGELLM_VL53L4CD_ULD_DIR}/platform.h")
        message(FATAL_ERROR
            "EDGELLM_VL53L4CD_ULD_DIR=${EDGELLM_VL53L4CD_ULD_DIR} must hold the ULD's "
            "api source and header and its platform.h; found api '${E06_ULD_API}', "
            "header '${E06_ULD_HDR}'. The ULD's own platform.c is never compiled here.")
    endif()
    list(GET E06_ULD_HDR 0 E06_ULD_HDR)
    get_filename_component(E06_ULD_HDR_NAME "${E06_ULD_HDR}" NAME)
    list(APPEND E06_SOURCES src/io_vl53l4cd.c src/vl53l4cd_platform.c
         ${E06_ULD_API} ${E06_ULD_CAL})
    set(E06_HAVE_ULD ON)
    message(STATUS "edgellm-e06: live sampler on, ULD from ${EDGELLM_VL53L4CD_ULD_DIR}, "
                   "header ${E06_ULD_HDR_NAME}, wait ${EDGELLM_VL53L4CD_WAIT_NAME}")
else()
    message(STATUS "edgellm-e06: replay only; the bench build needs "
                   "-DEDGELLM_VL53L4CD_ULD_DIR=<folder of the unpacked ULD>")
endif()

# The project owns main(): it forks the sampler and then calls edgellm_main(). The shared
# main is still compiled in, with its own main() left out by EDGELLM_NO_MAIN.
edgellm_add_project(edgellm-e06
    SOURCES ${E06_SOURCES}
    RULES src/rules_e06.c
    RULES_TEST src/rules_e06_test.c
    NO_MAIN)
target_sources(edgellm-e06 PRIVATE "${EDGELLM_ROOT}/common/c/src/edgellm_main.c")
target_compile_definitions(edgellm-e06 PRIVATE EDGELLM_NO_MAIN=1)

if(E06_HAVE_ULD)
    target_include_directories(edgellm-e06 PRIVATE "${EDGELLM_VL53L4CD_ULD_DIR}")
    target_compile_definitions(edgellm-e06 PRIVATE
        EDGELLM_HAVE_VL53L4CD=1
        EDGELLM_VL53L4CD_API_HEADER="${E06_ULD_HDR_NAME}"
        EDGELLM_VL53L4CD_WAIT_NAME=${EDGELLM_VL53L4CD_WAIT_NAME})
    # The ULD is compiled as shipped; its warnings are ST's, not this project's. Its api
    # source includes math.h.
    set_source_files_properties(${E06_ULD_API} ${E06_ULD_CAL} PROPERTIES COMPILE_OPTIONS "-w")
    target_link_libraries(edgellm-e06 PRIVATE m)
endif()

edgellm_add_oracle_test(edgellm-e06
    PROJECT e06
    REPLAY replay/approaches.csv
    GRAMMAR grammar.gbnf
    PROMPT prompt.txt
    EXPECTED "${EDGELLM_ROOT}/tests/oracle/e06.expected.csv")
