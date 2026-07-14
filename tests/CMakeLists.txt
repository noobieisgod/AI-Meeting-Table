set(AMT_TEST_CORE_SOURCES
    ${PROJECT_SOURCE_DIR}/src/app/application_context.cpp
    ${PROJECT_SOURCE_DIR}/src/app/mobile_app_controller.cpp
    ${PROJECT_SOURCE_DIR}/src/core/event_bus.cpp
    ${PROJECT_SOURCE_DIR}/src/core/logging.cpp
    ${PROJECT_SOURCE_DIR}/src/core/response_parser.cpp
    ${PROJECT_SOURCE_DIR}/src/core/session_runner.cpp
    ${PROJECT_SOURCE_DIR}/src/core/workflow_engine.cpp
    ${PROJECT_SOURCE_DIR}/src/persistence/database_manager.cpp
    ${PROJECT_SOURCE_DIR}/src/providers/provider_gateway.cpp
    ${PROJECT_SOURCE_DIR}/src/services/artifact_manager.cpp
    ${PROJECT_SOURCE_DIR}/src/services/budget_manager.cpp
    ${PROJECT_SOURCE_DIR}/src/services/credential_store.cpp
    ${PROJECT_SOURCE_DIR}/src/services/model_catalog_manager.cpp
    ${PROJECT_SOURCE_DIR}/src/services/upload_manager.cpp
)

add_library(AIMeetingTableTestCore STATIC ${AMT_TEST_CORE_SOURCES})
target_include_directories(AIMeetingTableTestCore PUBLIC ${PROJECT_SOURCE_DIR}/src)
target_link_libraries(AIMeetingTableTestCore PUBLIC
    Qt6::Core
    Qt6::Gui
    Qt6::Network
    Qt6::Sql
)
if (WIN32)
    target_link_libraries(AIMeetingTableTestCore PUBLIC Advapi32)
endif()

function(add_amt_test target source)
    add_executable(${target} ${source})
    target_link_libraries(${target} PRIVATE AIMeetingTableTestCore Qt6::Test)
    add_test(NAME ${target} COMMAND ${target})
    set_tests_properties(${target} PROPERTIES TIMEOUT 30)
endfunction()

add_amt_test(test_core test_core.cpp)
add_amt_test(test_database test_database.cpp)
add_amt_test(test_async test_async.cpp)
add_amt_test(test_model_catalog test_model_catalog.cpp)
add_amt_test(test_controller test_controller.cpp)

add_executable(test_qml test_qml.cpp)
target_link_libraries(test_qml PRIVATE Qt6::QuickTest)
target_compile_definitions(test_qml PRIVATE
    AMT_QML_TEST_SOURCE="${CMAKE_CURRENT_SOURCE_DIR}/qml/tst_transcript_scroll.qml.in"
    AMT_TRANSCRIPT_SCROLL_SOURCE="${PROJECT_SOURCE_DIR}/qml/TranscriptScroll.js"
)
add_test(NAME test_qml COMMAND test_qml)
set_tests_properties(test_qml PROPERTIES
    TIMEOUT 30
    ENVIRONMENT "QT_QPA_PLATFORM=offscreen"
)

add_test(
    NAME test_source_hygiene
    COMMAND ${CMAKE_COMMAND}
        -DSOURCE_DIR=${PROJECT_SOURCE_DIR}
        -P ${CMAKE_CURRENT_SOURCE_DIR}/verify_source_hygiene.cmake
)
