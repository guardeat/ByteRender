# Runs EXECUTABLE, which must terminate abnormally (abort), and checks that its output matches EXPECTED.
# CTest itself counts any crashed process as a failure, so death tests run through this script.
#   cmake -DEXECUTABLE=<path> -DEXPECTED=<regex> -P expect_abort.cmake

execute_process(
    COMMAND "${EXECUTABLE}"
    RESULT_VARIABLE result
    OUTPUT_VARIABLE output
    ERROR_VARIABLE output
)
# An abort is reported as a signal description on POSIX ("Subprocess aborted") and as exit code 3 on Windows; any
# other exit (including a non-zero return) is not an abort.
if(result MATCHES "^[0-9]+$" AND NOT (WIN32 AND result EQUAL 3))
    message(FATAL_ERROR "expected '${EXECUTABLE}' to abort, but it exited with code ${result}. Output:\n${output}")
endif()
if(NOT output MATCHES "${EXPECTED}")
    message(FATAL_ERROR "unexpected output from '${EXECUTABLE}' (result: ${result}):\n${output}")
endif()
