if(NOT DEFINED EXPECTED_COMMAND)
    message(FATAL_ERROR "EXPECTED_COMMAND is required")
endif()

set(command_args "${EXPECTED_COMMAND}")
if(DEFINED EXPECTED_ARGS)
    list(APPEND command_args ${EXPECTED_ARGS})
endif()

execute_process(
    COMMAND ${command_args}
    RESULT_VARIABLE expected_result
    OUTPUT_QUIET
    ERROR_QUIET
)

if("${expected_result}" STREQUAL "0")
    message(FATAL_ERROR "Expected command failure, but it succeeded: ${command_args}")
endif()

