# Building

## Windows

Install CMake, Visual Studio 2022 Build Tools, and Qt 6.8 or newer with the MSVC 2022 64-bit modules for Widgets, SQL, and Network. Run the commands from a shell where Qt is discoverable:

```powershell
cmake -S . -B build/windows -A x64 -DBUILD_TESTING=ON
cmake --build build/windows --config Release
```

The executable is created under `build/windows`. Use Qt's deployment tool when assembling a release package. Release packages must not contain libraries whose names end in `d`, Visual C++ Debug Runtime files, build-system output, or private symbols.

## Android

Install Qt 6.8 or newer for `android_arm64_v8a`, Android SDK platform 36, NDK 27.2.12479018, Ninja, and JDK 17. Define `QT_ROOT_DIR`, `ANDROID_SDK_ROOT`, and `ANDROID_NDK_ROOT` for your installation:

```powershell
& "$env:QT_ROOT_DIR\bin\qt-cmake.bat" -S . -B build/android -G Ninja `
  -DANDROID_SDK_ROOT="$env:ANDROID_SDK_ROOT" `
  -DANDROID_NDK_ROOT="$env:ANDROID_NDK_ROOT" `
  -DCMAKE_BUILD_TYPE=Release -DBUILD_TESTING=OFF
cmake --build build/android --target AIMeetingTable
```

The build fetches a pinned Android OpenSSL archive and verifies its SHA-256 digest. Android production signing is intentionally outside the repository and CI workflow.

## Troubleshooting

- Delete only the affected generated build directory when changing Qt kits, compilers, SDKs, or NDKs.
- Confirm that CMake resolves the intended Qt installation before diagnosing source failures.
- Use `ctest --output-on-failure` for native failures and Android `logcat` for device startup failures.
- Never add `local.properties`, keystores, credentials, or generated Android projects to source control.
