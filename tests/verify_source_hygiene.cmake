function(require_contains content expected description)
    string(FIND "${content}" "${expected}" position)
    if (position EQUAL -1)
        message(FATAL_ERROR "Missing ${description}: ${expected}")
    endif()
endfunction()

function(require_absent content forbidden description)
    string(FIND "${content}" "${forbidden}" position)
    if (NOT position EQUAL -1)
        message(FATAL_ERROR "Found forbidden ${description}: ${forbidden}")
    endif()
endfunction()

file(READ "${SOURCE_DIR}/src/providers/provider_gateway.cpp" provider_source)
require_absent("${provider_source}" "body=%1" "provider response-body diagnostic")
require_absent("${provider_source}" "sanitizedResponseBody" "response-body sanitizer that still logs private output")
require_absent("${provider_source}" "reply->errorString()" "raw provider transport diagnostic")
require_absent("${provider_source}" "const QByteArray fileBytes = file.readAll()" "whole-file Gemini upload buffering")
require_contains("${provider_source}" "performDeviceRequest(manager, uploadRequest, &file)" "streamed Gemini upload")

file(READ "${SOURCE_DIR}/src/services/model_catalog_manager.cpp" catalog_source)
require_absent("${catalog_source}" "reply->errorString()" "raw catalog transport diagnostic")
require_absent("${catalog_source}" "shortResponseBody" "catalog response-body diagnostic")
require_contains("${catalog_source}" "generation != m_generation" "stale catalog generation guard")

file(READ "${SOURCE_DIR}/android/src/com/aimeetingtable/mobile/FileBridge.java" file_bridge_source)
require_contains("${file_bridge_source}" "resolver.openInputStream(uri)" "input-first Android import")
require_contains("${file_bridge_source}" "target.delete()" "partial Android import cleanup")
string(FIND "${file_bridge_source}" "resolver.openInputStream(uri)" input_position)
string(FIND "${file_bridge_source}" "new FileOutputStream(target)" output_position)
if (output_position LESS input_position)
    message(FATAL_ERROR "Android attachment output is opened before its input stream")
endif()

file(READ "${SOURCE_DIR}/CMakeLists.txt" cmake_source)
require_contains("${cmake_source}" "com.aimeetingtable.myapp" "preserved Android application ID")
require_contains("${cmake_source}" "QT_ANDROID_MIN_SDK_VERSION 28" "Qt Android minimum SDK")
require_absent("${cmake_source}" "QT_ANDROID_MIN_SDK_VERSION 26" "obsolete Qt Android minimum SDK")
require_contains("${cmake_source}" "b71f1470962019bd89534a2919f5925f93bc5779" "pinned OpenSSL commit")
require_contains("${cmake_source}" "SHA256=6c1dfff0af367bba2f2c2a27f1c2061019537524dc7892ee8efbdc8e1c6224fa" "pinned OpenSSL archive hash")
require_contains("${cmake_source}" "EXCLUDE_BY_TYPE qmltooling" "Release QML tooling exclusion")

file(GLOB_RECURSE source_files LIST_DIRECTORIES false
    "${SOURCE_DIR}/src/*"
    "${SOURCE_DIR}/android/*"
    "${SOURCE_DIR}/qml/*"
    "${SOURCE_DIR}/tests/*"
    "${SOURCE_DIR}/CMakeLists.txt"
)
foreach(path IN LISTS source_files)
    if (path MATCHES "\\.(jks|keystore|p12|pfx|pem)$" OR path MATCHES "key\\.properties$")
        message(FATAL_ERROR "Signing material is present in source: ${path}")
    endif()
endforeach()
