# Mobile UI Documentation

## Phase 3 direction

The production Qt Quick interface follows the approved transcript-first Session direction. The primary destinations are, in order, Tables, Session, Event Log, and Settings. Phones use a bottom navigation bar with Settings at the far right. Wider windows use a sidebar with Settings as the final item. Artifacts remain part of Session rather than primary navigation.

Responsive behavior is implemented with native Qt Quick layouts. Phones use a single content column. Tablet and landscape layouts place the transcript beside a practical-width session panel. Both columns remain aligned to the top when extra vertical space is available.

## Tables

Tables presents existing meeting tables, their state, participants, and recent activity. The page provides create, open, duplicate, pin, rename, and delete actions through visible touch controls. Long titles and descriptions wrap or elide within their allocated space.

## Session

Session keeps the transcript as the primary workspace. The supporting column contains telemetry, Seats, Attachments, Generation, and Artifacts. The composer provides a taller multiline message field with compact attachment and Send controls.

The primary session action maps backend state to one visible control:

- Never started: Start
- Running: Pause
- Paused after running: Continue

Stop is a separate action. Stopping a previously run session preserves its prior-run state, so the next valid primary action is Continue rather than Start.

Seat cards open the seat editor. Each seat stores a name, role, provider, model, and accent color through the controller model. Changing a seat color updates existing transcript message accents immediately. Messages also retain speaker names and roles, so color is not the only identifier. Invalid color values use a visible fallback.

Attachments use the existing picker and backend import path. Android content is copied into app-private storage before hashing and upload. The backend owns attachment size and file-count safeguards. Generation and artifact controls use existing controller operations. Provider outcome-unknown states are shown without automatic replay.

## Event Log

Event Log presents session, provider, attachment, generation, and artifact events using the existing event model. Entries wrap long content and preserve state labels that do not rely on color alone.

## Settings

Settings is a full primary page with focused subsections:

- Providers: secure OpenAI, Gemini, and Anthropic credential entry and status
- Models: provider defaults and supported model choices
- Global hard stops: required backend-enforced budget, token, request, session, artifact, and timeout limits
- Attachment safeguards: backend-owned fixed limits and explanatory status
- Appearance: Light, Dark, or System appearance; Signal Session or Calm Workspace color theme; System, Workspace, or Console system-font preference

Persisted credentials use the existing secure credential store. QML receives provider status and a masked hint, never a complete saved key. Replacement fields start empty, use password masking, and do not log or copy credentials. Provider calls, hard-stop enforcement, attachment safeguards, and retry decisions remain in C++ and backend services.

Appearance changes update the interface without replacing active session state. The two color themes have intentional light and dark palettes. Font preferences use installed system font stacks and require no downloaded assets.

## Accessibility and responsive behavior

- Navigation follows the same Tables, Session, Event Log, Settings order visually and by keyboard.
- Qt Quick Controls provide semantic roles, keyboard focus, and touch-sized hit areas.
- Bottom navigation fits four destinations at phone width without horizontal scrolling.
- Transcript metadata reflows on narrow screens, and long names, roles, models, filenames, and messages wrap or elide intentionally.
- Dialogs have visible close or save actions and remain within the viewport.
- Seat identity, session state, validation, and provider status include text in addition to color.
- Light and dark palettes maintain readable foreground, surface, focus, and seat-accent combinations.
- Android Back closes a nested Settings subsection before leaving Settings. Dialogs provide explicit close or save controls.

The layout targets are 360 by 800, 412 by 915, 800 by 1280, and 1280 by 800. Desktop visual inspection covered a phone-sized 423 by 891 window and a wide 1707 by 960 window. The remaining exact target sizes are covered by responsive layout rules and require device or emulator confirmation.

## Validation and known limitations

Desktop Qt compilation, QML linting, controller tests, QML tests, and the full native test set are part of Phase 3 validation. On Windows, the QML test uses the native platform plugin because the offscreen plugin does not emit the required `windowShown` signal in this environment.

The Android arm64 debug build configures and packages with the repository's existing Qt, SDK, NDK, Gradle, and CMake process. No emulator or connected device was available during this validation, so Android launch, hardware Back behavior, and on-screen keyboard behavior still require device smoke testing before release. Release signing, packaging, tagging, and publication are outside this phase.
