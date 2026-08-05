# Architecture

AI Meeting Table uses one shared C++ backend with two Qt application layers.

## Layout

- `src/` contains domain models, workflow execution, persistence, provider clients, and shared services.
- `apps/windows/` contains the Qt Widgets entry point, UI, resources, and Windows application composition.
- `apps/android/` contains the Qt Quick entry point, QML UI, mobile controller, Android Java bridges, resources, and store assets.
- `tests/` exercises shared behavior through the Android controller on a desktop Qt runtime and includes QML and Java tests.
- `site/` contains the independently deployed privacy website.

## Data flow

The application controller loads persisted meeting state, the workflow engine selects work by phase and role, and the session runner sends provider requests through the provider gateway. Results update the transcript, budget state, event log, and artifact history before persistence.

Provider requests go directly from the application to OpenAI, Google Gemini, or Anthropic. Windows credentials use Credential Manager. Android credentials use the Android Keystore bridge. Meeting content, attachments, artifacts, and logs remain in platform-private local storage unless the user sends content to a provider.

## Compatibility

The repository migration does not intentionally change database schemas, provider request formats, or user-visible workflow rules. Platform-specific UI and credential code remain isolated while shared backend behavior has one implementation.
