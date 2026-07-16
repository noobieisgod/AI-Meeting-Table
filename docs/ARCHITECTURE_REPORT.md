# AI Meeting Table Android Architecture Report

## Project Split

`Source Code` is the untouched Windows desktop baseline. `Mobile Source Code` is the Android migration copy and is the only tree modified for Android.

The mobile copy removes copied Windows build and installer artifacts. The uncompiled legacy Widgets source files were removed after confirming that the mobile target and runtime entry points do not reference them. The separate desktop source tree remains the reference implementation for the Windows UI.

## Framework And Build Detection

- Original framework: Qt6 Widgets desktop application.
- Mobile framework: Qt Quick/QML with C++ back-end services.
- Original build system: CMake, C++20, `Qt6::Widgets`, `Qt6::Sql`, `Qt6::Network`.
- Mobile build system: CMake, C++20, `Qt6::Core`, `Qt6::Gui`, `Qt6::Quick`, `Qt6::QuickControls2`, `Qt6::Sql`, `Qt6::Network`.
- Original entry point: `src/main.cpp` with `QApplication`, splash screen, and `MainWindow`.
- Mobile entry point: `src/main.cpp` with `QGuiApplication`, `QQmlApplicationEngine`, and `MobileAppController`.
- Android package source: `android/AndroidManifest.xml`, Android Java bridge classes under `android/src/com/aimeetingtable/mobile`.

## Major Modules

- Application context: owns database, workflow, session runner, providers, credentials, upload, artifacts, model catalog, and app settings.
- Mobile controller: exposes tables, seats, transcript, artifacts, logs, settings, credentials, attachments, and session actions to QML.
- Domain model: stores phases, roles, providers, seats, transcript entries, logs, artifacts, budgets, stop policy, and session state.
- Persistence: SQLite via `QSqlDatabase`, app-private path from `QStandardPaths::AppDataLocation`.
- Workflow: event-driven phase engine with Research, Planning, Execution, Quality Control, Present, terminal states, arbitration, pause, resume, and continuation.
- Providers: OpenAI, Gemini, and Anthropic through `QNetworkAccessManager`, JSON requests, multipart uploads, model refresh, and token usage.
- Services: credential storage, upload hashing, artifact file creation, budget enforcement, model catalog refresh.
- UI: QML mobile shell with table drawer, session workspace, touch seat visualization, transcript, artifacts, logs, composer, attachment import, seat editor, and settings dialog.

## Data Flow

1. QML calls `MobileAppController`.
2. Controller mutates or queries `ApplicationContext`.
3. Session actions call `SessionRunner`.
4. `SessionRunner` publishes workflow events to `WorkflowEngine`.
5. Provider requests are dispatched through `ProviderGateway`.
6. Provider responses update transcript, budget tallies, logs, artifacts, and attachment provider handles.
7. Controller emits change signals to refresh QML models.
8. `DatabaseManager` persists session state, transcript, logs, and artifacts to SQLite.

## Threading And Background Work

- Provider calls run in dedicated `QThread` workers to preserve parallel Research phase behavior.
- Each provider request uses a local `QEventLoop` and timeout timer inside its worker thread.
- Session elapsed time uses a `QTimer` in `SessionRunner` only while at least one session is active.
- Runner state persistence is coalesced once per event-loop turn, while lifecycle flushes remain synchronous.
- Provider requests carry a per-session run generation so late responses from a stopped or restarted run cannot modify the new run.
- Model catalog refreshes cancel superseded requests, enforce a timeout, and retain the last successful catalog on refresh failure.

## Storage And Security

- SQLite database, imported attachments, and artifacts use Android app-private storage.
- Windows Credential Manager is replaced on Android by a Java Android Keystore bridge.
- Non-Android fallback uses `QSettings` only for desktop compile checks of the mobile copy.
- Attachments selected through Android content URIs are copied into app-private storage through `FileBridge`.
- Attachment files are removed only after persistence succeeds, only when no table still references them, and only when their canonical path remains inside the managed attachment directory.

## Feature Inventory And Compatibility

| Feature | Android status | Notes |
| --- | --- | --- |
| Multiple meeting tables | Requires adaptation | Desktop list/sidebar becomes drawer and mobile list. |
| Create, duplicate, rename, pin, delete tables | Requires adaptation | Implemented through QML drawer controls. |
| Seat configuration | Requires redesign | Desktop grid/dialog becomes touch editor dialog. |
| 1 to 8 seat visualization | Requires redesign | Custom QWidget paint is replaced by QML seat layout. |
| Provider selection | Fully compatible | Existing provider enum and model catalog are reused. |
| Model selection and refresh | Requires adaptation | Existing network refresh is reused and exposed to QML. |
| Effort controls | Fully compatible | Existing model effort mapping is reused. |
| Role validation | Fully compatible | Existing role validation is reused in controller. |
| Final Decision Maker | Fully compatible | Existing workflow role semantics remain. |
| Phase workflow | Fully compatible | Existing event engine and runner are preserved. |
| Research parallelism | Fully compatible | Existing provider worker threads are preserved. |
| Transcript | Requires adaptation | Desktop QTextBrowser rendering becomes QML feed. |
| User message queueing | Fully compatible | Existing queued input behavior is preserved. |
| Artifacts | Requires adaptation | Artifact files remain Markdown in app-private storage, viewed as a mobile list. |
| Event log | Requires adaptation | Desktop log pane becomes QML tab. |
| Attachments | Requires adaptation | Desktop paths are imported into app-private storage. |
| Provider file upload handles | Fully compatible | Existing provider handle JSON remains on attachments. |
| API key storage | Requires redesign | Windows Credential Manager becomes Android Keystore. |
| Global hard stops | Requires adaptation | Exposed through mobile settings controls. |
| Per-table hard stops | Requires adaptation | Data model supports it, detailed mobile editor can be expanded from current global editor. |
| Pause, resume, stop, continuation | Fully compatible | Existing runner calls are exposed to QML. |
| Theme | Requires adaptation | QML owns presentation; stored theme state remains available. |
| Desktop splitters and context menus | Requires redesign | Replaced by drawer, tabs, dialogs, and direct actions. |
| Windows installer packaging | Not applicable | Removed from mobile tree. |
| Google Play AAB | Requires adaptation | Android manifest and CMake Android properties are added. |

## Android Compatibility Risks

- Android package verification still requires physical phone and tablet testing because no device is currently connected to this machine.
- Provider APIs and model names may change over time. The existing dynamic model refresh mitigates this but does not remove provider policy risk.
- Long provider requests can continue while the app is backgrounded. The current implementation persists state but does not add Android foreground service behavior.
- The QML UI is a functional first Android shell. Further visual QA on real phone and tablet hardware is required before Play submission.
