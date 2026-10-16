# Runs one project over its replay file with the stub and checks the log against the
# oracle file. Invoked by ctest through edgellm_add_oracle_test; see
# cmake/edgellm_project.cmake. Variables: BIN CHECK PROJECT REPLAY GRAMMAR PROMPT
# EXPECTED LOG EXTRA.
separate_arguments(EXTRA_LIST NATIVE_COMMAND "${EXTRA}")
execute_process(
    COMMAND "${BIN}" --project "${PROJECT}" --replay "${REPLAY}" --grammar "${GRAMMAR}"
            --prompt "${PROMPT}" --log "${LOG}" --stub ${EXTRA_LIST}
    RESULT_VARIABLE run_rc)
if(NOT run_rc EQUAL 0)
    message(FATAL_ERROR "${PROJECT}: the replay run exited with ${run_rc}")
endif()
execute_process(
    COMMAND "${CHECK}" "${LOG}" "${EXPECTED}"
    RESULT_VARIABLE check_rc)
if(NOT check_rc EQUAL 0)
    message(FATAL_ERROR "${PROJECT}: the log differs from ${EXPECTED} (exit ${check_rc})")
endif()
