# Testing

Configure a Windows build with `BUILD_TESTING=ON`, build it, and run:

```powershell
ctest --test-dir build/windows -C Release --output-on-failure
```

The suite covers workflow and budget rules, database persistence, asynchronous session behavior, provider responses, model catalogs, controller behavior, startup timing, QML scrolling, Android attachment streaming, and source hygiene.

Before releasing:

- Run the complete test suite from a fresh build directory.
- Launch the Windows package on a clean supported system.
- Install the Android release candidate on a phone and tablet.
- Exercise credential save and removal, provider refresh, attachment import, session continuation, persistence after restart, and artifact opening.
- Confirm logs do not contain credentials, raw provider response bodies, or private attachment contents.
