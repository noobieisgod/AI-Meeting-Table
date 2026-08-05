if (NOT DEFINED JAVAC OR NOT DEFINED JAVA OR NOT DEFINED PRODUCTION_SOURCE
    OR NOT DEFINED TEST_SOURCE OR NOT DEFINED OUTPUT_DIR)
    message(FATAL_ERROR "Attachment import Java test configuration is incomplete")
endif()

file(REMOVE_RECURSE "${OUTPUT_DIR}")
file(MAKE_DIRECTORY "${OUTPUT_DIR}")

execute_process(
    COMMAND "${JAVAC}" -Xlint:all -d "${OUTPUT_DIR}" "${PRODUCTION_SOURCE}" "${TEST_SOURCE}"
    RESULT_VARIABLE compile_result
    OUTPUT_VARIABLE compile_output
    ERROR_VARIABLE compile_error
)
if (NOT compile_result EQUAL 0)
    message(FATAL_ERROR "Attachment import Java compile failed:\n${compile_output}\n${compile_error}")
endif()

execute_process(
    COMMAND "${JAVA}" -ea -cp "${OUTPUT_DIR}" com.aimeetingtable.mobile.BoundedStreamCopierTest
    RESULT_VARIABLE test_result
    OUTPUT_VARIABLE test_output
    ERROR_VARIABLE test_error
)
if (NOT test_result EQUAL 0)
    message(FATAL_ERROR "Attachment import Java tests failed:\n${test_output}\n${test_error}")
endif()
message(STATUS "${test_output}")
