# FetchContent invokes this only on its build-local dependency checkout.
execute_process(COMMAND "${GIT}" apply --reverse --check "${PATCH}"
    WORKING_DIRECTORY "${SOURCE_DIR}" RESULT_VARIABLE already_applied
    OUTPUT_QUIET ERROR_QUIET)
if(already_applied EQUAL 0)
    return()
endif()
execute_process(COMMAND "${GIT}" apply --check "${PATCH}"
    WORKING_DIRECTORY "${SOURCE_DIR}" RESULT_VARIABLE checked
    OUTPUT_VARIABLE output ERROR_VARIABLE errors)
if(NOT checked EQUAL 0)
    message(FATAL_ERROR "Dependency patch does not match the pinned source: ${output}${errors}")
endif()
execute_process(COMMAND "${GIT}" apply "${PATCH}"
    WORKING_DIRECTORY "${SOURCE_DIR}" RESULT_VARIABLE applied
    OUTPUT_VARIABLE output ERROR_VARIABLE errors)
if(NOT applied EQUAL 0)
    message(FATAL_ERROR "Dependency patch failed: ${output}${errors}")
endif()
