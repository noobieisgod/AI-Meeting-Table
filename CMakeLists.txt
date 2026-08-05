cmake_minimum_required(VERSION 3.21)

project(AIMeetingTable VERSION 1.0.0 LANGUAGES CXX)

include(CTest)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

set(AMT_SHARED_SOURCES
    src/core/event_bus.h
    src/core/event_bus.cpp
    src/core/logging.h
    src/core/logging.cpp
    src/core/response_parser.h
    src/core/response_parser.cpp
    src/core/session_runner.h
    src/core/session_runner.cpp
    src/core/startup_timeline.h
    src/core/startup_timeline.cpp
    src/core/workflow_engine.h
    src/core/workflow_engine.cpp
    src/domain/models.h
    src/persistence/database_manager.h
    src/persistence/database_manager.cpp
    src/providers/provider_gateway.h
    src/providers/provider_gateway.cpp
    src/services/artifact_manager.h
    src/services/artifact_manager.cpp
    src/services/budget_manager.h
    src/services/budget_manager.cpp
    src/services/credential_store.h
    src/services/credential_store.cpp
    src/services/model_catalog_manager.h
    src/services/model_catalog_manager.cpp
    src/services/upload_manager.h
    src/services/upload_manager.cpp
)

if(ANDROID)
    find_package(Qt6 6.8 REQUIRED COMPONENTS Core Gui Quick QuickControls2 Sql Network)
    qt_standard_project_setup()
    qt_policy(SET QTP0001 NEW)
    qt_policy(SET QTP0002 NEW)
    qt_policy(SET QTP0004 NEW)

    qt_add_executable(AIMeetingTable
        ${AMT_SHARED_SOURCES}
        apps/android/src/main.cpp
        apps/android/src/app/application_context.h
        apps/android/src/app/application_context.cpp
        apps/android/src/app/mobile_app_controller.h
        apps/android/src/app/mobile_app_controller.cpp
        src/services/attachment_import_manager.h
        src/services/attachment_import_manager.cpp
        apps/android/resources.qrc
    )
    target_include_directories(AIMeetingTable PRIVATE src apps/android/src)
    target_link_libraries(AIMeetingTable PRIVATE
        Qt6::Core Qt6::Gui Qt6::Quick Qt6::QuickControls2 Qt6::Sql Qt6::Network
    )

    qt_add_qml_module(AIMeetingTable
        URI AIMeetingTable
        VERSION 1.0
        QML_FILES
            apps/android/qml/Main.qml
            apps/android/qml/TablesPage.qml
            apps/android/qml/TranscriptScroll.js
    )

    include(FetchContent)
    FetchContent_Declare(android_openssl
        URL https://codeload.github.com/KDAB/android_openssl/tar.gz/b71f1470962019bd89534a2919f5925f93bc5779
        URL_HASH SHA256=6c1dfff0af367bba2f2c2a27f1c2061019537524dc7892ee8efbdc8e1c6224fa
        DOWNLOAD_EXTRACT_TIMESTAMP TRUE
    )
    FetchContent_MakeAvailable(android_openssl)
    include("${android_openssl_SOURCE_DIR}/android_openssl.cmake")

    set_target_properties(AIMeetingTable PROPERTIES
        QT_ANDROID_PACKAGE_SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/apps/android/android"
        QT_ANDROID_PACKAGE_NAME "com.aimeetingtable.myapp"
        QT_ANDROID_TARGET_SDK_VERSION 36
        QT_ANDROID_MIN_SDK_VERSION 28
        QT_ANDROID_ABIS "arm64-v8a"
        QT_ANDROID_VERSION_NAME "${PROJECT_VERSION}"
        QT_ANDROID_VERSION_CODE 4
    )
    add_android_openssl_libraries(AIMeetingTable)

    if(CMAKE_BUILD_TYPE MATCHES "^(Release|RelWithDebInfo|MinSizeRel)$")
        qt_import_plugins(AIMeetingTable EXCLUDE_BY_TYPE qmltooling)
    endif()
elseif(WIN32)
    find_package(Qt6 6.8 REQUIRED COMPONENTS Core Gui Widgets Sql Network)
    qt_standard_project_setup()

    set(AMT_WINDOWS_RESOURCE)
    if(MSVC)
        list(APPEND AMT_WINDOWS_RESOURCE apps/windows/app_icon.rc)
    endif()

    qt_add_executable(AIMeetingTable WIN32
        ${AMT_SHARED_SOURCES}
        apps/windows/src/main.cpp
        apps/windows/src/app/application_context.h
        apps/windows/src/app/application_context.cpp
        apps/windows/src/ui/main_window.h
        apps/windows/src/ui/main_window.cpp
        apps/windows/src/ui/dialogs/create_table_dialog.h
        apps/windows/src/ui/dialogs/create_table_dialog.cpp
        apps/windows/src/ui/dialogs/seat_editor_dialog.h
        apps/windows/src/ui/dialogs/seat_editor_dialog.cpp
        apps/windows/src/ui/dialogs/settings_dialog.h
        apps/windows/src/ui/dialogs/settings_dialog.cpp
        apps/windows/src/ui/widgets/meeting_table_widget.h
        apps/windows/src/ui/widgets/meeting_table_widget.cpp
        apps/windows/resources.qrc
        ${AMT_WINDOWS_RESOURCE}
    )
    target_include_directories(AIMeetingTable PRIVATE src apps/windows/src)
    target_link_libraries(AIMeetingTable PRIVATE
        Qt6::Core Qt6::Gui Qt6::Widgets Qt6::Sql Qt6::Network Advapi32
    )
else()
    message(FATAL_ERROR "AI Meeting Table currently supports Windows and Android targets")
endif()

if(BUILD_TESTING AND NOT ANDROID)
    find_package(Qt6 6.8 REQUIRED COMPONENTS Test Quick QuickTest)
    add_subdirectory(tests)
endif()
