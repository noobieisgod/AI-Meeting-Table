# Android Test Plan

## Automated Host Regression Suite

Configure and run the isolated Qt Test suite with the desktop Qt kit:

```powershell
$env:PATH = "C:\QtFresh\Tools\mingw1310_64\bin;C:\QtFresh\6.11.1\mingw_64\bin;" + $env:PATH
C:\QtFresh\6.11.1\mingw_64\bin\qt-cmake.bat `
  -G Ninja `
  -DCMAKE_MAKE_PROGRAM=C:/QtFresh/Tools/Ninja/ninja.exe `
  -DCMAKE_CXX_COMPILER=C:/QtFresh/Tools/mingw1310_64/bin/g++.exe `
  -DCMAKE_BUILD_TYPE=Debug `
  -DBUILD_TESTING=ON `
  -S . `
  -B build-tests
cmake --build build-tests
ctest --test-dir build-tests --output-on-failure
```

The suite covers domain serialization and role validation, workflow and arbitration transitions, final-line ruling parsing, duplicate and stale provider responses, transcript scroll restoration, transcript-copy formatting, budget boundaries, SQLite migration and incremental child writes, deterministic ordering, overlapping and timed-out model refreshes, granular controller signals, managed attachment cleanup, and diagnostic source hygiene. Fake network replies are used, so the suite does not read real credentials or call provider APIs.

## Build Verification

- Configure with a desktop Qt kit for compile validation and automated tests.
- Configure with a Qt Android arm64-v8a kit.
- Build Debug APK.
- Build Release APK.
- Generate an unsigned Release AAB for local verification, then sign through the protected release workflow.
- Inspect manifest for target SDK, min SDK, package id, permissions, and exported activity.

## Functional Testing

- Create an empty table, then add up to 8 seats individually.
- Search, select, duplicate, rename, pin, unpin, and delete tables.
- Configure occupied and empty seats.
- Validate exactly one Final Decision Maker when running.
- Select OpenAI, Gemini, and Anthropic providers.
- Select model and effort levels.
- Save and reload provider API keys.
- Refresh model catalog after saving keys.
- Send user messages.
- Run full sessions through Research, Planning, Execution, Quality Control, Present, and Completed.
- Confirm Planning arbitration `PROCEED` advances to Execution without completing the meeting.
- Confirm Quality Control arbitration `REVISE` returns to Execution rather than repeating Quality Control against an unchanged artifact.
- Confirm Quality Control arbitration `PROCEED` advances to Present and Present `PROCEED` completes the meeting.
- Stop a session manually.
- Pause and resume during delayed turns.
- Trigger continuation prompts through low budgets or time limits.
- Queue user input during a running session.
- Generate and display artifact entries.
- Display logs and transcript after restart.

## Attachment Testing

- Import a text file.
- Import an image.
- Import a PDF.
- Confirm imported files are copied into app-private storage.
- Confirm hashes are generated.
- Confirm provider upload handles are saved after successful provider requests.
- Remove an attachment before running.
- Add an attachment during a running session and confirm it is queued for the next phase.

## Android Lifecycle Testing

- Rotate phone portrait to landscape and back.
- Rotate tablet landscape to portrait and back.
- Background the app during an idle session.
- Background the app during an in-flight provider request.
- Return from background and confirm state refreshes.
- Kill and relaunch the app after transcript, seat, and settings changes.
- Confirm selected table restoration.

## UI And Accessibility Testing

- Verify phone portrait layout at 360 dp width.
- Verify tablet split layout at 800 dp width or greater.
- Verify text wrapping in transcript cards and seat buttons.
- Verify long model names remain within seat cards and the full value remains available in the seat editor.
- Verify transcript text can be selected without preventing normal vertical scrolling.
- Verify `Copy Full Transcript` preserves entry order and metadata.
- Verify new entries follow when near the bottom and preserve position when scrolled upward.
- Verify transcript scroll positions remain independent when switching tables.
- Verify large touch targets for seats, run, pause, stop, send, and attachment.
- Verify screen reader labels for symbolic buttons.
- Verify light theme contrast.
- Verify software keyboard does not block the composer.

## Failure Testing

- Missing API key.
- Invalid API key.
- Provider HTTP 400, 401, 429, and 500 responses.
- Network unavailable.
- Provider timeout.
- Attachment import failure.
- SQLite initialization failure.
- Empty provider response.
- Duplicate provider response for an already-consumed request id.
- Provider response from a previous phase, round, or run generation.
- Multiple explicit `FINAL_RULING` lines with contradictory values.
- Budget limits and safety reserve limits.
