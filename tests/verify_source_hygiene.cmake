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
file(READ "${SOURCE_DIR}/android/src/com/aimeetingtable/mobile/BoundedStreamCopier.java" stream_copier_source)
require_contains("${file_bridge_source}" "IMPORT_EXECUTOR.submit" "background Android attachment executor")
require_contains("${file_bridge_source}" "resolver.openInputStream(uri)" "Android content-provider stream")
require_contains("${file_bridge_source}" "cancelImport" "Android attachment cancellation")
require_contains("${file_bridge_source}" "lastProgressNanos" "Android no-progress watchdog")
require_absent("${file_bridge_source}" "public static String importUriToPrivateFile" "legacy synchronous string import result")
require_contains("${stream_copier_source}" "MAX_ATTACHMENT_BYTES = 25L * 1024L * 1024L" "25 MiB attachment limit")
require_contains("${stream_copier_source}" "FREE_SPACE_RESERVE_BYTES = 64L * 1024L * 1024L" "64 MiB free-space reserve")
require_contains("${stream_copier_source}" "NO_PROGRESS_TIMEOUT_NANOS = 60L * 1_000_000_000L" "60-second no-progress deadline")
require_contains("${stream_copier_source}" "copied > limits.maximumBytes - read" "authoritative streaming byte limit")
require_contains("${stream_copier_source}" "finalName + \".part\"" "partial attachment destination")
require_contains("${stream_copier_source}" "digest.update(buffer, 0, read)" "incremental attachment hashing")
require_contains("${stream_copier_source}" "StandardCopyOption.ATOMIC_MOVE" "atomic attachment finalization")

file(READ "${SOURCE_DIR}/src/services/attachment_import_manager.cpp" import_manager_source)
require_contains("${import_manager_source}" "QThread::create" "dedicated C++ attachment worker")
require_contains("${import_manager_source}" "QCryptographicHash::Sha256" "incremental local attachment hashing")
file(READ "${SOURCE_DIR}/src/app/mobile_app_controller.cpp" controller_source)
require_absent("${controller_source}" "importAttachmentToPrivateStorage" "legacy controller-thread attachment copy")
require_contains("${controller_source}" "result.sha256, result.byteCount" "precomputed attachment metadata handoff")

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
