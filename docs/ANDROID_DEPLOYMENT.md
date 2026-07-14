# Android Build And Deployment Guide

## Prerequisites

Install:
- Qt 6.8 or newer Android kit, including `android_arm64_v8a`. The verified local kit is `C:\QtFresh\6.11.1\android_arm64_v8a`.
- Android SDK with platform API 35 or newer. This machine has API 36 installed, so target SDK 36 is used.
- Android NDK version matching the installed Qt Android kit.
- JDK supported by the Android Gradle Plugin bundled with the Qt kit.

Verified local Android tooling:
- Qt: `C:\QtFresh\6.11.1\android_arm64_v8a`
- Android SDK: `C:\Users\Andy\AppData\Local\Android\Sdk`
- Android NDK: `C:\Users\Andy\AppData\Local\Android\Sdk\ndk\27.2.12479018`
- Android platform: `android-36`
- Build tools: `37.0.0`

`JAVA_HOME` is not currently set, but Java is installed at `C:\Program Files\Java\jdk-23`.

## Configure Android Debug Build

From a Qt Android-enabled shell:

```powershell
$env:ANDROID_SDK_ROOT = "C:\Users\Andy\AppData\Local\Android\Sdk"
$env:ANDROID_HOME = $env:ANDROID_SDK_ROOT
$env:ANDROID_NDK_ROOT = "C:\Users\Andy\AppData\Local\Android\Sdk\ndk\27.2.12479018"
C:\QtFresh\6.11.1\android_arm64_v8a\bin\qt-cmake.bat `
  -G Ninja `
  -DCMAKE_BUILD_TYPE=Debug `
  -S "C:\Users\Andy\Desktop\AI Meeting Table\Mobile Source Code" `
  -B "C:\Users\Andy\Desktop\AI Meeting Table\Mobile Source Code\build-android" `
```

## Debug APK

```powershell
cmake --build "C:\Users\Andy\Desktop\AI Meeting Table\Mobile Source Code\build-android"
```

Verified debug APK output:

```text
C:\Users\Andy\Desktop\AI Meeting Table\Mobile Source Code\build-android\android-build\build\outputs\apk\debug\android-build-debug.apk
```

## Startup Crash Debugging

Use adb logcat before guessing at Android startup failures:

```powershell
$adb = "C:\Users\Andy\AppData\Local\Android\Sdk\platform-tools\adb.exe"
& $adb logcat -c
& $adb shell am force-stop com.aimeetingtable.myapp
& $adb shell am start -W -n com.aimeetingtable.myapp/org.qtproject.qt.android.bindings.QtActivity
Start-Sleep -Seconds 5
& $adb logcat -d -v time | Out-File -LiteralPath "C:\Tmp\aimeetingtable-startup-logcat.txt" -Encoding utf8
& $adb shell pidof com.aimeetingtable.myapp
```

Useful filters:

```powershell
Select-String -LiteralPath "C:\Tmp\aimeetingtable-startup-logcat.txt" -Pattern `
  "QtLoader","QQml","QML","Main.qml","main\(\) returned","VM exiting","has died","FATAL"
```

The Samsung Galaxy S24 Ultra startup failure was:

```text
E/QtLoader: The main library name is null or empty.
```

Root cause: the custom Android manifest did not include the QtActivity metadata that names the native entry library. The app exited before `main.cpp` and before QML could run.

Fixes applied:
- `android\AndroidManifest.xml` now declares `org.qtproject.qt.android.bindings.QtApplication`.
- `android\AndroidManifest.xml` now sets `android.app.lib_name` to `AIMeetingTable`.
- `android\AndroidManifest.xml` now sets `android.app.arguments` to an empty string.
- `qml\Main.qml` was checked after the native loader fix. The attempted `QtQuick.Accessibility` import is not available in the Qt Android package and is not used.

Verified fixed startup log:

```text
Load ... libAIMeetingTable_arm64-v8a.so ... ok
Displayed com.aimeetingtable.myapp/org.qtproject.qt.android.bindings.QtActivity
```

The process remained alive after launch:

```text
adb shell pidof com.aimeetingtable.myapp
23782
```

The captured verification log is:

```text
C:\Tmp\aimeetingtable-startup-logcat-fixed-2.txt
```

## Configure Optimized Release Build

```powershell
C:\QtFresh\6.11.1\android_arm64_v8a\bin\qt-cmake.bat `
  -G Ninja `
  -DCMAKE_BUILD_TYPE=Release `
  -S "C:\Users\Andy\Desktop\AI Meeting Table\Mobile Source Code" `
  -B "C:\Users\Andy\Desktop\AI Meeting Table\Mobile Source Code\build-android-release"

cmake --build "C:\Users\Andy\Desktop\AI Meeting Table\Mobile Source Code\build-android-release"
```

## Signing Key

Create a release keystore outside the repository:

```powershell
keytool -genkeypair `
  -v `
  -keystore "C:\Users\Andy\Desktop\AI Meeting Table\release-key.jks" `
  -alias ai-meeting-table `
  -keyalg RSA `
  -keysize 4096 `
  -validity 10000
```

Do not commit keystores or passwords.

## Android App Bundle

Build the release Android App Bundle:

```powershell
cmake --build "C:\Users\Andy\Desktop\AI Meeting Table\Mobile Source Code\build-android-release" --target aab
```

The release AAB currently builds successfully at:

```text
C:\Users\Andy\Desktop\AI Meeting Table\Mobile Source Code\build-android-release\android-build\build\outputs\bundle\release\android-build-release.aab
```

Use Qt Creator's Android build settings or the Qt-generated Gradle project to create a signed release AAB. Set:
- Package id: `com.aimeetingtable.myapp`
- Min SDK: 26
- Target SDK: 36 on this machine, satisfying the Google Play API 35 or newer requirement
- ABI: `arm64-v8a`
- Signing config: release keystore created above

The generated `.aab` is the artifact to upload to Google Play.

Current remaining release step: configure a real upload/release keystore. The generated release AAB has been verified as unsigned with `jarsigner`.

## Launcher Icon

The Android launcher icon uses the existing project icon:

```text
C:\Users\Andy\Desktop\AI Meeting Table\Mobile Source Code\Icon Logo.png
```

`Full Logo.png` was not used because it is a full logo or wordmark style asset. `Icon Logo.png` is the same asset already referenced by `resources.qrc` and `main.cpp`.

Icon resources added:
- `android\res\mipmap-mdpi\ic_launcher.png`
- `android\res\mipmap-hdpi\ic_launcher.png`
- `android\res\mipmap-xhdpi\ic_launcher.png`
- `android\res\mipmap-xxhdpi\ic_launcher.png`
- `android\res\mipmap-xxxhdpi\ic_launcher.png`
- matching `ic_launcher_round.png` and `ic_launcher_foreground.png` files for each density
- `android\res\mipmap-anydpi-v26\ic_launcher.xml`
- `android\res\mipmap-anydpi-v26\ic_launcher_round.xml`
- `android\res\values\colors.xml`

The manifest points to:

```xml
android:icon="@mipmap/ic_launcher"
android:roundIcon="@mipmap/ic_launcher_round"
```

APK badging verification:

```text
application-label:'AI Meeting Table'
application-icon-160:'res/mipmap-anydpi-v26/ic_launcher.xml'
application-icon-240:'res/mipmap-anydpi-v26/ic_launcher.xml'
application-icon-320:'res/mipmap-anydpi-v26/ic_launcher.xml'
application-icon-480:'res/mipmap-anydpi-v26/ic_launcher.xml'
application-icon-640:'res/mipmap-anydpi-v26/ic_launcher.xml'
```

Modified source files for the startup and icon fix:
- `android\AndroidManifest.xml`
- `android\res\values\colors.xml`
- `android\res\mipmap-*`
- `android\res\mipmap-anydpi-v26\ic_launcher.xml`
- `android\res\mipmap-anydpi-v26\ic_launcher_round.xml`
- `docs\ANDROID_DEPLOYMENT.md`

## Mobile Tab Layout

The main QML layout was redesigned for phone screens on 2026-06-09.

Modified files:
- `qml\Main.qml`
- `docs\ANDROID_DEPLOYMENT.md`

Layout approach:
- Replaced the desktop-style `SplitView` main layout with a `ColumnLayout`.
- The main page area is a `StackLayout` that fills the space above the bottom navigation.
- The bottom navigation is a fixed `TabBar` with four destinations: `Table`, `Transcript`, `Artifacts`, and `Log`.
- `selectedTab` is the shared state for both `StackLayout.currentIndex` and `TabBar.currentIndex`.
- Each destination renders as a full-screen page. The table, transcript, artifacts, and log are not shown together on phone screens.

Destination behavior:
- `Table` shows the status/phase area, meeting table visualization, and run/pause/continue/stop controls.
- `Transcript` shows the transcript list and the attachment/composer/send controls.
- `Artifacts` shows only artifact rows.
- `Log` shows only log rows.

Mobile spacing and constraint checks:
- `SplitView`, `SplitView.fillWidth`, `SplitView.fillHeight`, and `SplitView.preferredWidth` were removed from `qml\Main.qml`.
- The old split-pane preferred width logic was removed.
- The bottom `TabBar` sits outside the scrollable page content, so it does not scroll away and does not overlay the pages.
- The Table page uses internal `ScrollView` content with explicit left/right insets.

Menu and settings fixes:
- The table drawer button now uses a QML-drawn hamburger icon while preserving `tableDrawer.open()`.
- The settings dialog remains centered and fixed.
- The settings dialog uses an internal `Flickable` content item so only settings content scrolls.
- The settings panel bottom edge and Close action remain visible on the Samsung Galaxy S24 Ultra.

Settings centering correction:
- `qml\Main.qml` now parents the settings `Dialog` to `Overlay.overlay`.
- The dialog x/y coordinates are calculated from guarded overlay width and height values, not from the page content area.
- The outer dialog no longer uses a manual root-height based height binding.
- The internal `Flickable` height is capped from the overlay height, with a fallback to the root height while the popup parent is initializing.
- This avoids the previous low placement caused by centering a dialog whose visual chrome and content item did not match the manual root-height calculation.

Verification performed:
- Debug APK rebuilt successfully.
- Debug APK installed successfully on the connected Samsung Galaxy S24 Ultra.
- App launched successfully and remained running.
- Logcat showed `libAIMeetingTable_arm64-v8a.so` loading and Android reporting the activity as displayed.
- Visual screenshots confirmed the Table page uses the full phone width, the bottom tabs switch between isolated pages, the drawer opens from the hamburger button, and the settings panel is centered.
- Follow-up verification confirmed the settings panel is vertically centered against the full app overlay and no longer sits near the bottom.
- Release AAB rebuilt successfully.

Verification files:

```text
C:\Tmp\aimeetingtable-tab-layout-logcat-2.txt
C:\Tmp\aimeetingtable-screens\foreground-table.png
C:\Tmp\aimeetingtable-screens\foreground-transcript.png
C:\Tmp\aimeetingtable-screens\foreground-artifacts.png
C:\Tmp\aimeetingtable-screens\foreground-log.png
C:\Tmp\aimeetingtable-screens\drawer.png
C:\Tmp\aimeetingtable-screens\settings-dialog-2.png
C:\Tmp\aimeetingtable-screens\settings-centered-final.png
C:\Tmp\aimeetingtable-settings-centered-final-logcat.txt
```

Latest build outputs:

```text
C:\Users\Andy\Desktop\AI Meeting Table\Mobile Source Code\build-android\android-build\build\outputs\apk\debug\android-build-debug.apk
C:\Users\Andy\Desktop\AI Meeting Table\Mobile Source Code\build-android\android-build\build\outputs\bundle\release\android-build-release.aab
```

Known limitation:
- The release AAB still needs a real upload/release keystore before it is production-ready for Google Play.

## Provider Diagnostics And Mobile Usability Fixes

Implemented on 2026-06-09.

Modified files:
- `qml\Main.qml`
- `src\app\mobile_app_controller.h`
- `src\app\mobile_app_controller.cpp`
- `src\services\model_catalog_manager.h`
- `src\services\model_catalog_manager.cpp`
- `src\providers\provider_gateway.cpp`
- `src\core\session_runner.h`
- `src\core\session_runner.cpp`
- `src\domain\models.h`
- `docs\ANDROID_DEPLOYMENT.md`

API key UI changes:
- The mobile settings panel now has explicit provider fields for `OpenAI`, `Google`, and `Anthropic`.
- The `Refresh Models` action saves the visible provider key fields before starting refresh.
- Android secure storage is unchanged and still uses the Android Keystore bridge through `SecureCredentialStore.java`.
- Status text only reports whether a key is saved with a short masked indicator. Full API keys are not written to logs or documentation.

Model refresh behavior:
- OpenAI refresh uses `GET https://api.openai.com/v1/models`.
- Google refresh uses `GET https://generativelanguage.googleapis.com/v1beta/models` and filters to Gemini models that support `generateContent`.
- Anthropic refresh uses `GET https://api.anthropic.com/v1/models`.
- Each provider now reports provider-specific refresh status, including skipped no-key, loaded count, HTTP failure, network failure, parse failure, and fallback use.
- If Anthropic refresh fails or returns no usable models, the app loads the static Anthropic fallback list and labels that result as fallback.
- Stale static fallback IDs such as `gpt-5.4-mini` and `claude-opus-4.7` were removed or mapped to safer fallback aliases.

Network and API debugging changes:
- Provider request failures now include provider name, model ID, endpoint type, key-present state, send stage, HTTP status, network error, SSL errors, and a short sanitized response body.
- Gemini URLs with API keys are not logged.
- Full request bodies and full API keys are not logged.
- Android deployment settings show the Qt OpenSSL TLS plugin is packaged:

```text
C:\QtFresh\6.11.1\android_arm64_v8a\plugins\tls\libplugins_tls_qopensslbackend_arm64-v8a.so
```

Table failure behavior:
- Research batches now track how many provider requests were dispatched and how many failed.
- If every Research request fails, the Log tab receives `All model requests failed during Research. Stopping table.` and the table stops with that reason.
- Partial Research failures still preserve the existing skip-and-continue behavior.

Mobile UI changes:
- The Transcript composer is now internally scrollable, so multi-line goals remain usable in the fixed composer area.
- Seat 1 and Seat 5 received a targeted radial offset in the meeting table visualization so they sit farther from the table center without resizing the whole table or changing seat bindings.

Verification commands:

```powershell
cmake --build "C:\Users\Andy\Desktop\AI Meeting Table\Mobile Source Code\build-android" --config Debug
cmake --build "C:\Users\Andy\Desktop\AI Meeting Table\Mobile Source Code\build-android-release" --target aab
```

Build results:
- Debug APK rebuilt successfully.
- Release AAB rebuilt successfully.

Output paths:

```text
C:\Users\Andy\Desktop\AI Meeting Table\Mobile Source Code\build-android\android-build\build\outputs\apk\debug\android-build-debug.apk
C:\Users\Andy\Desktop\AI Meeting Table\Mobile Source Code\build-android\android-build\build\outputs\bundle\release\android-build-release.aab
```

Device verification:
- The rebuilt debug APK installed successfully on the connected Samsung Galaxy S24 Ultra.
- Device id: `R5CX129D9JZ`
- Model: `SM_S9280`
- The app launched with a cold start and remained running.
- Focused startup error log capture was empty.
- Log file: `C:\Tmp\aimeetingtable-provider-fix-startup-logcat.txt`
- The backpack checklist provider runtime test was not run from Codex because it requires entering provider API keys and starting a real provider-backed session on the device UI.

Device verification commands:

```powershell
& "C:\Users\Andy\AppData\Local\Android\Sdk\platform-tools\adb.exe" logcat -c
& "C:\Users\Andy\AppData\Local\Android\Sdk\platform-tools\adb.exe" install -r "C:\Users\Andy\Desktop\AI Meeting Table\Mobile Source Code\build-android\android-build\build\outputs\apk\debug\android-build-debug.apk"
& "C:\Users\Andy\AppData\Local\Android\Sdk\platform-tools\adb.exe" shell am start -W -n com.aimeetingtable.myapp/org.qtproject.qt.android.bindings.QtActivity
& "C:\Users\Andy\AppData\Local\Android\Sdk\platform-tools\adb.exe" logcat Qt:E AndroidRuntime:E libc:E JNI:E QML:E AIMeetingTable:E *:S
```

Known limitations:
- Runtime API success still depends on valid provider API keys, account billing/access, and model availability returned by each provider for that account.
- Release AAB signing still needs a real upload/release keystore before Google Play production use.

## Android TLS And OpenSSL Deployment

Implemented on 2026-06-09.

Root cause:
- Android Refresh Models failed for OpenAI, Google, and Anthropic with `TLS initialization failed`.
- The APK already contained Qt Network and the Qt OpenSSL TLS backend plugin.
- The APK did not contain the OpenSSL runtime libraries required by that backend for `arm64-v8a`.
- This was below the provider API layer, so provider JSON, model IDs, and API keys were not the primary cause.

Modified files:
- `CMakeLists.txt`
- `src\main.cpp`
- `docs\ANDROID_DEPLOYMENT.md`

OpenSSL deployment fix:
- Added Qt's recommended KDAB `android_openssl` CMake integration through `FetchContent`.
- Included `android_openssl.cmake`.
- Added `add_android_openssl_libraries(AIMeetingTable)` for Android builds.
- Reconfigured `build-android` so `androiddeployqt` regenerated `android-AIMeetingTable-deployment-settings.json`.
- The deployment settings now include:

```text
C:\Users\Andy\Desktop\AI Meeting Table\Mobile Source Code\build-android\_deps\android_openssl-src\ssl_3\arm64-v8a\libcrypto_3.so
C:\Users\Andy\Desktop\AI Meeting Table\Mobile Source Code\build-android\_deps\android_openssl-src\ssl_3\arm64-v8a\libssl_3.so
```

APK contents verified:

```text
lib/arm64-v8a/libQt6Network_arm64-v8a.so
lib/arm64-v8a/libplugins_tls_qopensslbackend_arm64-v8a.so
lib/arm64-v8a/libcrypto_3.so
lib/arm64-v8a/libssl_3.so
```

Runtime SSL diagnostic logging:
- Startup now logs `QSslSocket::supportsSsl()`.
- Startup logs SSL build version, runtime version, active backend, available backends, and CPU architecture.
- No API keys or provider payloads are logged.

Device verification on Samsung Galaxy S24 Ultra:
- Device id: `R5CX129D9JZ`
- Model: `SM_S9280`
- Debug APK installed successfully.
- App cold-launched successfully and stayed running.
- Runtime SSL startup log showed:

```text
Qt SSL supported: true
Qt SSL build version: OpenSSL 3.5.4 30 Sep 2025
Qt SSL runtime version: OpenSSL 3.1.8 11 Feb 2025
Qt SSL active backend: openssl
Qt SSL available backends: openssl
```

Refresh Models verification:
- Tapped `Refresh Models` on the device settings panel.
- The previous `TLS initialization failed` status did not return.
- Provider model refresh statuses showed:

```text
OpenAI models loaded: 77.
Google models loaded: 20.
Anthropic models loaded: 14.
```

Verification files:

```text
C:\Tmp\aimeetingtable-tls-startup-logcat.txt
C:\Tmp\aimeetingtable-refresh.xml
C:\Tmp\aimeetingtable-tls-refresh-logcat.txt
```

Latest build outputs:

```text
C:\Users\Andy\Desktop\AI Meeting Table\Mobile Source Code\build-android\android-build\build\outputs\apk\debug\android-build-debug.apk
C:\Users\Andy\Desktop\AI Meeting Table\Mobile Source Code\build-android\android-build\build\outputs\bundle\release\android-build-release.aab
```

Remaining limitations:
- The backpack checklist session was not run from Codex because the current visible table had empty seats and configuring/running a full provider-backed table through UI automation would be fragile.
- Real meeting responses still require valid API keys, account billing/access, supported selected models, and occupied seat configuration.

## Composer, English Output, And Markdown Rendering

Implemented on 2026-06-09.

Modified files:
- `qml\Main.qml`
- `src\app\mobile_app_controller.cpp`
- `src\providers\provider_gateway.cpp`
- `docs\ANDROID_DEPLOYMENT.md`

Message input styling fix:
- The Transcript composer keeps its internal `ScrollView` so multi-line input can scroll.
- The composer now has a visible rounded white background and subtle border around the scrollable text area.
- Attachment and Send button behavior were not changed.

Default English prompt behavior:
- Shared provider prompt assembly now includes:

```text
Respond in English unless the user explicitly asks for another language.
```

- The instruction is added in `buildPromptText()` and therefore applies consistently to OpenAI, Google, Anthropic, FDM, lead roles, and regular participants.
- Translation or non-English tasks remain allowed when the user explicitly asks for another language.

Markdown rendering approach:
- Transcript model and user-facing message bodies now use Qt Quick `Text` with `Text.MarkdownText`.
- Artifact rows now expose a capped markdown preview from the artifact `.md` file and render it with `Text.MarkdownText`.
- Logs remain plain text.
- A previous display-only `TextEdit.MarkdownText` attempt was replaced with lighter `Text.MarkdownText` after Android produced a native rendering crash.
- Link activation handlers are intentionally no-op, so links do not trigger navigation or remote loading from model output.

Artifact content exposure:
- `MobileAppController::artifactSummary()` now reads up to 65536 bytes from each artifact markdown file.
- Truncated previews append a plain truncation notice.

Verification performed:
- Debug APK rebuilt successfully.
- Release AAB rebuilt successfully.
- Debug APK installed successfully on Samsung device `R5CX129D9JZ`.
- App cold-launched successfully and stayed running.
- Focused startup error log was empty:

```text
C:\Tmp\aimeetingtable-markdown-startup-logcat-fixed.txt
```

- Transcript UI dump confirmed `Add attachment`, scrollable `Message`, and `Send` controls remain present.
- Screenshot confirmed the composer border/background is visible:

```text
C:\Tmp\aimeetingtable-transcript-composer.png
```

- Refresh Models regression check still showed:

```text
OpenAI models loaded: 77.
Google models loaded: 20.
Anthropic models loaded: 14.
```

Latest build outputs:

```text
C:\Users\Andy\Desktop\AI Meeting Table\Mobile Source Code\build-android\android-build\build\outputs\apk\debug\android-build-debug.apk
C:\Users\Andy\Desktop\AI Meeting Table\Mobile Source Code\build-android\android-build\build\outputs\bundle\release\android-build-release.aab
```

Known limitations:
- Full backpack checklist generation was not run from Codex because the visible table did not have configured occupied seats.
- Qt built-in Markdown rendering is used as-is. Remote image loading from model output is not intentionally enabled.

## Empty States, Keyboard Dismissal, And FDM Rulings

Implemented on 2026-06-09.

Modified files:
- `qml\Main.qml`
- `android\AndroidManifest.xml`
- `src\providers\provider_gateway.cpp`
- `src\core\session_runner.cpp`
- `docs\ANDROID_DEPLOYMENT.md`

Empty-state behavior:
- Transcript, Artifacts, and Log now show a centered empty state when their backing list is empty.
- The empty state text is:

```text
Nothing Here Yet
Run the table to generate content.
```

- Transcript keeps the composer visible while the empty state appears only in the message list area.
- Empty states are hidden automatically once their list contains content.

Android keyboard dismissal fix:
- The Qt activity now declares `android:windowSoftInputMode="adjustResize"` in `android\AndroidManifest.xml`.
- After Send succeeds, the composer clears its text, clears focus, explicitly hides the input method, and gives focus back to the root window on the next QML event turn.
- No keyboard spacers, fake overlays, insecure network settings, or hardcoded screen heights were added.

FDM structured ruling format:
- Final decision prompts now require an explicit final line:

```text
FINAL_RULING: APPROVE
FINAL_RULING: REVISE
FINAL_RULING: STOP
```

- `APPROVE` means the result is good enough to deliver or proceed.
- `REVISE` means specific changes are required before delivery.
- `STOP` means end early.
- Prompts now instruct the final decision maker not to say the result should proceed while issuing `REVISE`.

FDM parsing behavior:
- Provider response parsing now prefers the last explicit `FINAL_RULING:` field.
- Legacy first-line parsing remains as a fallback for older responses.
- `PROCEED`, `ACCEPT`, `FINAL`, and `READY` are accepted as approve aliases.
- Explanatory prose never overrides a valid explicit ruling. If more than one explicit ruling line is present, the final valid line wins and a content-free diagnostic is recorded.
- If no final ruling can be parsed, the workflow logs a clear parsing failure and uses the existing safe fallback path without creating a revise loop.

Verification performed:
- Debug APK rebuilt successfully.
- Release AAB rebuilt successfully.
- Debug APK installed successfully on Samsung device `R5CX129D9JZ`.
- App cold-launched successfully and stayed running.
- Focused QML, Qt, JNI, libc, AndroidRuntime, and app error log was empty after launch and after the keyboard smoke test.
- UI hierarchy checks confirmed `Nothing Here Yet` and `Run the table to generate content.` on Transcript, Artifacts, and Log when empty.
- Screenshot verification confirmed the keyboard dismissal state repainted normally, with no stale black area after the keyboard was hidden.
- A Send smoke test confirmed the transcript updated, the composer cleared, the keyboard remained dismissed, and no focused startup/UI errors were logged.

Verification files:

```text
C:\Tmp\aimeetingtable-empty-keyboard-fdm-logcat.txt
C:\Tmp\aimeetingtable-empty-keyboard-fdm-logcat-after-ui.txt
C:\Tmp\aimeetingtable-transcript.xml
C:\Tmp\aimeetingtable-artifacts.xml
C:\Tmp\aimeetingtable-log.xml
C:\Tmp\aimeetingtable-keyboard-dismissed.png
C:\Tmp\aimeetingtable-after-send-button.png
```

Latest build outputs:

```text
C:\Users\Andy\Desktop\AI Meeting Table\Mobile Source Code\build-android\android-build\build\outputs\apk\debug\android-build-debug.apk
C:\Users\Andy\Desktop\AI Meeting Table\Mobile Source Code\build-android\android-build\build\outputs\bundle\release\android-build-release.aab
```

Known limitations:
- Full FDM provider workflow verification was not run from Codex because the visible table on the device had empty seats and configuring a full provider-backed run through UI automation would be fragile.
- FDM reliability still depends on model compliance, but explicit final rulings are authoritative and conflicting ruling lines are reported without recording response content.

## Android Keyboard Composer Visibility

Implemented on 2026-06-09.

Modified files:
- `android\AndroidManifest.xml`
- `qml\Main.qml`
- `docs\ANDROID_DEPLOYMENT.md`

Final soft-input approach:
- The Qt activity now uses `android:windowSoftInputMode="adjustPan"`.
- `adjustResize` was tested but Samsung Keyboard continued to overlay the Qt surface in this build.
- A QML-only composer lift using `Qt.inputMethod.keyboardRectangle` was tested and removed because the IME rectangle and focus state changed during the keyboard animation, causing the composer to drop back under the keyboard.
- `adjustPan` was selected as the practical fallback requested by the user: Android pans the app upward so the message composer remains visible while typing.

Composer behavior:
- The message box, attach button, and Send button are visible above Samsung Keyboard when the keyboard is open.
- The Transcript page still uses the existing mobile tab layout and composer styling.
- Send still calls `hideKeyboardAfterSend()`, which clears composer focus, hides the input method, and returns focus to the root window.

Verification performed:
- Debug APK rebuilt successfully.
- Release AAB rebuilt successfully.
- Debug APK installed successfully on Samsung device `R5CX129D9JZ`.
- Keyboard-open screenshot confirmed the app pans upward and the composer is visible above the keyboard.
- Focused Qt, QML, JNI, libc, AndroidRuntime, and app logcat output was empty after the keyboard test.

Verification files:

```text
C:\Tmp\aimeetingtable-keyboard-adjustpan.png
C:\Tmp\aimeetingtable-keyboard-adjustpan-logcat.txt
```

Latest build outputs:

```text
C:\Users\Andy\Desktop\AI Meeting Table\Mobile Source Code\build-android\android-build\build\outputs\apk\debug\android-build-debug.apk
C:\Users\Andy\Desktop\AI Meeting Table\Mobile Source Code\build-android\android-build\build\outputs\bundle\release\android-build-release.aab
```

Known limitations:
- UI automation could not reliably activate the visually panned Send button, so Send-path dismissal should be manually verified on the device.
- Dismissing Samsung Keyboard with the Android Back key can leave the Qt surface visually panned with black space below until focus/layout is reset. The app's Send path still explicitly clears focus and hides the keyboard.

## Production Meeting Prompt Rewrite

Implemented on 2026-06-10.

Modified files:
- `src\providers\provider_gateway.cpp`
- `src\core\session_runner.cpp`
- `docs\ANDROID_DEPLOYMENT.md`

Prompt architecture:
- `buildPromptText()` now assembles the API prompt as two conceptual sections: stable behavior and dynamic context.
- The provider payload shape is unchanged. OpenAI, Gemini, and Anthropic still receive one composed prompt string in the existing user message or content part.
- Stable behavior covers language, phase discipline, authority boundaries, research policy, the required `Research used:` line, style rules, evaluation rules, MLA guidance, and history priority.
- Dynamic context includes the seat display name, model display name, table title, phase, role, research tool availability, phase purpose, authority reminder, roster, latest user message, turn instruction, artifact summary, artifact content, and conversation history.
- Conversation history is explicitly described as context, not as higher priority instruction.

Phase and authority behavior:
- Research requires independent evidence gathering and forbids final results, plans, QC rulings, and final decisions.
- Planning forms a plan without creating the final result. The Lead Planner owns the plan unless a concrete, evidence-backed flaw is identified.
- Execution creates or revises the official artifact. Only the Lead Executioner owns the official artifact.
- Quality Control reviews and verifies without rewriting unless instructed. Only the Lead Quality Control reviewer may issue the final QC ruling.
- Final Decision is reserved for the Final Decision Maker.

FDM ruling behavior:
- Final Decision Maker prompts continue to require exactly one final parseable line:

```text
FINAL_RULING: APPROVE
FINAL_RULING: REVISE
FINAL_RULING: STOP
```

- `APPROVE` means the current artifact is good enough to proceed or deliver.
- `REVISE` means specific required changes remain, and the explanation must name them.
- `STOP` means continuing is not useful, and the explanation must say why.
- Parser behavior remains compatible with the existing `FINAL_RULING:` path and legacy first-line aliases, while explicit final rulings remain authoritative.

Research reporting:
- Every substantive model response is instructed to include exactly one short `Research used:` line.
- Gemini Research turns report that Gemini Google Search is available because the existing provider layer attaches the search tool only in Research.
- OpenAI and Anthropic turns report that no research tools are available in the current payload shape.

Provider documentation notes:
- OpenAI Responses supports system and developer roles and structured outputs, but this pass keeps the current single composed prompt string.
- Gemini supports `system_instruction`, `contents`, Google Search tools, and structured JSON output, but this pass keeps the current `contents[0].parts` shape.
- Anthropic Messages supports a top-level `system` prompt and message content, and its prompting guidance recommends clear instructions and explicit output formats, but this pass keeps the current user message content shape.

Static verification:
- Old phase-purpose and authority prompt fragments were removed.
- The corrupted token discipline text was replaced.
- Prompt-facing truncation text no longer contains corrupted question-mark markers.
- The rewritten prompt strings contain no em dash character.

Build verification:
- Debug APK rebuilt successfully.
- Release AAB rebuilt successfully.
- Device install was not run from Codex because `adb devices -l` returned no connected devices.

Latest build outputs:

```text
C:\Users\Andy\Desktop\AI Meeting Table\Mobile Source Code\build-android\android-build\build\outputs\apk\debug\android-build-debug.apk
C:\Users\Andy\Desktop\AI Meeting Table\Mobile Source Code\build-android\android-build\build\outputs\bundle\release\android-build-release.aab
```

Known limitations:
- Provider-native system or developer message fields were not implemented in this pass.
- Native structured output for FDM rulings was not added. The app continues to use the existing `FINAL_RULING:` parser.
- History compaction remains unchanged. If prompt growth becomes a problem, add bounded history or summarization as a separate change.

## Log Wrapping, Gemini Diagnostics, And Early Arbitration

Implemented on 2026-06-10.

Modified files:
- `qml\Main.qml`
- `src\app\mobile_app_controller.cpp`
- `src\providers\provider_gateway.cpp`
- `src\core\session_runner.cpp`
- `src\core\workflow_engine.cpp`
- `docs\ANDROID_DEPLOYMENT.md`

Log tab behavior:
- The Log tab now uses a custom wrapping card delegate instead of a one-line `ItemDelegate`.
- Log summaries render as plain text with `wrapMode: Text.Wrap`.
- Log rows grow vertically based on content, so long provider errors are readable without UI ellipses.
- The log model now exposes log type and actor name for a compact metadata header.

Gemini diagnostics:
- Gemini generateContent failures include `toolsEnabled=yes/no` and `tool=google_search` or `tool=none`.
- The Gemini REST request uses the official `google_search` tool field.
- Provider failure logs still include provider, model, key presence, send stage, HTTP status, network error, SSL errors, and sanitized response body.
- API keys and full request bodies are not logged.

Early arbitration behavior:
- Early Final Decision Maker arbitration now uses phase-specific rulings:

```text
FINAL_RULING: PROCEED
FINAL_RULING: REVISE
FINAL_RULING: STOP
```

- `PROCEED` advances the workflow to the next phase instead of completing the session.
- Planning arbitration can proceed to Execution.
- Quality Control arbitration can proceed to Present.
- Present-phase final decisions still use:

```text
FINAL_RULING: APPROVE
FINAL_RULING: REVISE
FINAL_RULING: STOP
```

- Multiple explicit FDM ruling lines now add a safe visible log entry and use the final valid line.
- Arbitration requests are logged with the phase and the reason that disagreement markers were detected.

ADB diagnostics:

```powershell
$adb = "C:\Users\Andy\AppData\Local\Android\Sdk\platform-tools\adb.exe"
& $adb devices -l
& $adb logcat -d Qt:E AndroidRuntime:E libc:E JNI:E QML:E AIMeetingTable:E *:S
```

Harmless verification prompt:

```text
Create a short markdown checklist for packing a school backpack for tomorrow.
```

Known limitations:
- In-app provider logs are persisted in the app database and are not always mirrored to Android `logcat`.
- Backend response bodies are still length-capped after sanitization for safety.

Build and device verification:
- Debug APK rebuilt successfully.
- Release AAB rebuilt successfully.
- Debug APK installed successfully on Samsung device `R5CX129D9JZ`.
- App cold-launched successfully and stayed running.
- Focused Qt, QML, JNI, libc, AndroidRuntime, and app error log was empty after launch.

Latest build outputs:

```text
C:\Users\Andy\Desktop\AI Meeting Table\Mobile Source Code\build-android\android-build\build\outputs\apk\debug\android-build-debug.apk
C:\Users\Andy\Desktop\AI Meeting Table\Mobile Source Code\build-android\android-build\build\outputs\bundle\release\android-build-release.aab
```

## Google Play Preparation

Before upload:
- Confirm the AAB targets Android 15, API level 35, or newer.
- Verify permissions are limited to internet and network state unless new features require more.
- Confirm privacy policy text covers direct provider API calls, API key storage, attachments, and generated artifacts.
- Verify app label, launcher icon, screenshots, feature graphic, content rating, and data safety form.
- Test on at least one phone and one tablet before internal testing rollout.

## Official References

- Google Play target API requirement: https://developer.android.com/google/play/requirements/target-sdk
- Qt Android deployment: https://doc.qt.io/qt-6.5/deployment-android.html
- Qt Android CMake build notes: https://doc.qt.io/qt-6.5/android-building-user-projects.html

## Mobile Session Persistence

Implemented on 2026-06-10.

Persistence save fix updated on 2026-06-10.

Modified files:
- `src\app\application_context.h`
- `src\app\application_context.cpp`
- `src\app\mobile_app_controller.h`
- `src\app\mobile_app_controller.cpp`
- `src\persistence\database_manager.cpp`
- `src\persistence\database_manager.h`
- `src\services\artifact_manager.cpp`
- `src\main.cpp`
- `docs\ANDROID_DEPLOYMENT.md`

Root cause:
- The `meeting_tables` insert path listed 30 columns but used 31 value placeholders.
- SQLite rejected every new table insert with `Parameter count mismatch`.
- Because table rows were never inserted, startup loaded zero tables and created a new empty sample table on every launch.
- After the placeholder fix, a second save blocker appeared because null `QString` values were bound into NOT NULL text columns on new empty tables.
- The issue was not caused by debug APK status, Android app-private storage, `adb install -r`, or package id changes.

Fix:
- The `meeting_tables` insert statement now has matching column, placeholder, and bind-value counts.
- A row-existence check now runs when `UPDATE` affects zero rows, so insertion happens only when the table id is actually absent.
- NOT NULL text fields are normalized to non-null empty strings before binding.
- Save diagnostics now report database file state and row counts after successful saves.
- Startup diagnostics now report database file state before load and restored row counts.
- Restore selection now prefers the saved current table id. If stale, it chooses the newest table with content, then the newest table.
- Artifact creation logs path, existence, and file size after writing the markdown file.

Persistence model:
- Session state continues to use the existing SQLite database at Android app-private storage:

```text
QStandardPaths::AppDataLocation/ai_meeting_table.db
```

- Artifact content continues to use app-private markdown files:

```text
QStandardPaths::AppDataLocation/artifacts/<artifact-version-id>.md
```

- The database persists table metadata, transcript entries, artifact metadata, log events, attachments, queued input ids, current artifact id, budget state, token and cost counters, phase state, seat settings, and table settings.
- API keys remain in the secure credential store and are not written to the session database.
- Provider request bodies, auth headers, and in-flight network internals are not persisted.

Restore behavior:
- Startup loads all saved tables with transcripts, artifacts, and logs.
- The Android app restores `mobile/currentTableId` when that table still exists.
- If the saved id is missing or stale, the app selects the newest table with content, then the most recently updated table.
- A sample table is created only when no saved tables exist.
- Restore diagnostics log database path, file existence, file size, table count, selected table id, transcript count, artifact count, and log count.
- Artifact metadata is retained if the matching markdown file is missing. The Log tab receives a safe warning and the artifact view falls back to unavailable content.

Lifecycle save behavior:
- The selected session is flushed when Android reports inactive, suspended, hidden, or app quit states.
- Normal session updates are still saved through the existing `SessionRunner::sessionStateChanged` path.
- User messages, attachments, table edits, run state updates, transcript changes, artifact metadata, and log entries are saved through the same SQLite path.
- Current table id is synced through `QSettings` after selection and lifecycle flush.

Useful diagnostics:

```powershell
$adb = "C:\Users\Andy\AppData\Local\Android\Sdk\platform-tools\adb.exe"
& $adb logcat -d Qt:I Qt:W AndroidRuntime:E libc:E JNI:E QML:E AIMeetingTable:I *:S
```

Expected startup log fields:
- `Database open: path=...`
- `Database file state: context=before load path=... exists=... size=...`
- `Database load table: id=... transcript=... artifacts=... logs=...`
- `Database load summary: tables=... totalTranscript=... totalArtifacts=... totalLogs=...`
- `Persistence restore: loaded tables=...`
- `Persistence restore: saved selected table id=... exists=...`
- `Persistence restore: selected table=... transcript=... artifacts=... logs=...`
- `Database row counts: context=after save table=... tables=... transcript=... artifacts=... logs=...`
- `Persistence flush: table=... saved=true transcript=... artifacts=... logs=...`

Actions that preserve app-private data:
- Normal close and reopen.
- Android recents swipe away followed by reopen.
- Android force stop followed by reopen.
- `adb install -r` over the same package id:

```powershell
adb install -r "C:\Users\Andy\Desktop\AI Meeting Table\Mobile Source Code\build-android\android-build\build\outputs\apk\debug\android-build-debug.apk"
```

Actions that delete app-private data:
- Uninstalling the app.
- Clearing app data from Android settings.
- Installing under a different package id.

Verification prompt:

```text
Create a short markdown checklist for packing a school backpack for tomorrow.
```

Verification checklist:
- Send a message, close the app, reopen, and confirm the message remains in Transcript.
- Run or stop a table, close the app, reopen, and confirm Transcript, Artifacts, Log, phase, round, tokens, cost, current artifact id, attachments, and table settings remain.
- Background the app and reopen it without losing the current table.
- Force stop the app and launch again without losing local session history.
- Confirm artifact markdown content renders from app-private storage without network access.

Build and device verification:
- Debug APK rebuilt successfully.
- Release AAB rebuilt successfully.
- Debug APK installed successfully on Samsung device `R5CX129D9JZ`.
- App cold-launched successfully and stayed running.
- App-private storage inspection confirmed `ai_meeting_table.db`, WAL files, `artifacts`, and `settings` exist under `files`.
- `adb install -r` preserved the package data directory.
- Cold launch after the fix saved the initial table successfully with `tables=1`.
- Force stop and cold relaunch loaded the saved table successfully with `tables=1`.
- No `Parameter count mismatch` or `NOT NULL constraint failed` errors appeared after the final fix.

Latest build outputs:

```text
C:\Users\Andy\Desktop\AI Meeting Table\Mobile Source Code\build-android\android-build\build\outputs\apk\debug\android-build-debug.apk
C:\Users\Andy\Desktop\AI Meeting Table\Mobile Source Code\build-android\android-build\build\outputs\bundle\release\android-build-release.aab
```

Known limitations:
- Uninstalling the app or clearing Android app data removes the database, artifacts, attachments, and settings.
- In-flight provider requests are not restored after process death. Persisted transcript, artifacts, logs, and table state are restored.

## Manual-Test Workflow and Transcript Follow-Up

FDM ruling semantics:
- Planning arbitration `PROCEED` advances exactly one phase to Execution.
- Quality Control arbitration `PROCEED` advances exactly one phase to Present.
- Quality Control arbitration `REVISE` returns to Execution so the official artifact can be corrected before another review.
- Present `PROCEED` remains an approval alias and completes the meeting.
- New ruling transcript entries state the arbitration phase and destination so an advance is not mistaken for final approval.
- The final valid explicit `FINAL_RULING` line is authoritative. Explanatory prose cannot override it.
- Multiple explicit ruling lines use the final valid line and add a content-free diagnostic to the Log tab.

Provider response lifecycle:
- Every dispatched request records its session, seat, phase, round, generation, mode, and token reservation.
- Pending context is removed before a response is processed.
- Unknown, duplicate, generation-stale, phase-stale, round-stale, session-mismatched, and seat-mismatched responses cannot add transcript entries or advance the workflow.

Transcript and seat UI:
- New messages follow automatically only while the transcript is within 64 logical pixels of the bottom.
- A scrolled-up position is restored only after the model count matches and delegate-derived content height is stable across two timer passes.
- Scroll positions are retained independently per table.
- Transcript message text is selectable and a user-invoked `Copy Full Transcript` action copies ordered plain text with time, speaker, phase, and round metadata.
- Seat names use one-line elision; model names wrap to two lines and elide inside the existing card bounds.
