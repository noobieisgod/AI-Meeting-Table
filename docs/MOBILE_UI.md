# Mobile UI Documentation

## Phase 3 direction

The production Qt Quick interface follows the approved transcript-first Session direction. The primary destinations are, in order, Tables, Session, Event Log, and Settings. Phones use a bottom navigation bar with Settings at the far right. Wider windows use a sidebar with Settings as the final item. Artifacts remain part of Session rather than primary navigation.

Responsive behavior is implemented with native Qt Quick layouts. Phones use a single content column. Tablet and landscape layouts place the transcript beside a practical-width session panel. Both columns remain aligned to the top when extra vertical space is available.

## Tables

Tables presents existing meeting tables, their state, participants, and recent activity. The page provides create, open, duplicate, pin, rename, and delete actions through visible touch controls. Long titles and descriptions wrap or elide within their allocated space.

## Session

Session keeps the transcript as the primary workspace. The supporting column contains telemetry, Seats, Attachments, Generation, and Artifacts. The composer provides a taller multiline message field with compact attachment and Send controls. It grows to a practical limit, then scrolls internally so long instructions remain editable without hiding the actions.

The primary session action maps backend state to one visible control:

- Never started: Start
- Running: Pause
- Paused after running: Continue

Stop is a separate action. Stopping a previously run session preserves its prior-run state, so the next valid primary action is Continue rather than Start.

Seat cards open the seat editor. Newly added seats are active by default, so Add Seat no longer exposes the stored occupied flag. Each seat stores a name, role, provider, model, and accent color through the controller model. The normal editor offers Blue, Cyan, Green, Amber, Orange, Red, Purple, and Pink with named swatches. Existing custom hexadecimal colors remain visible as Custom until the user selects a preset. Changing a seat color updates existing transcript message accents immediately. Messages also retain speaker names and roles, so color is not the only identifier. Invalid color values use a visible fallback.

Attachments use the existing picker beside the composer and the backend import path. The Session attachment panel lists imported files and no longer duplicates the import action. Android content is copied into app-private storage before hashing and upload. The backend owns attachment size and file-count safeguards. As a post-test enhancement, tapping a completed attachment asks Android to open a protected, read-only content URI through a compatible application. Raw file URIs and unrestricted filesystem paths are not exposed. Generation and artifact controls use existing controller operations. Provider outcome-unknown states are shown without automatic replay.

Provider adapters accept only provider-specific user-visible response fields. OpenAI output text and supported refusal blocks are extracted explicitly. Anthropic text blocks are extracted while thinking, signatures, metadata, and other non-visible blocks are ignored. A malformed response or a response without visible content is recorded as a redacted failure and pauses the recoverable session instead of entering opaque data into the transcript or completing the phase. Outcome-unknown requests remain paused for explicit user continuation and are never automatically replayed.

Usage telemetry prefers final provider-reported input and output counts. When usage must be estimated, the token display says Approx. Cached or otherwise incomplete pricing makes the cost display unavailable rather than presenting false precision. Known partial usage from a definite provider failure is retained once, while outcome-unknown usage remains unconfirmed. Backend hard stops continue to own enforcement.

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
- Android top and bottom surfaces extend to the physical edges while their controls use Qt safe-area margins for status bars, cutouts, gesture navigation, and navigation bars.

The layout targets are 360 by 800, 412 by 915, 800 by 1280, and 1280 by 800. Desktop visual inspection covered a phone-sized 423 by 891 window and a wide 1707 by 960 window. The remaining exact target sizes are covered by responsive layout rules and require device or emulator confirmation.

## Validation and known limitations

Desktop Qt compilation, QML linting, controller tests, QML tests, and the full native test set are part of Phase 3 validation. On Windows, the QML test uses the native platform plugin because the offscreen plugin does not emit the required `windowShown` signal in this environment.

The Android arm64 debug build configures and packages with the repository's existing Qt, SDK, NDK, Gradle, and CMake process. The post-test correction build completed successfully with Qt 6.11.1 and target API 36. No emulator, AVD, or connected device was enumerated during this continuation, so Pixel 10 Pro safe-area behavior, the Android attachment chooser, hardware Back behavior, orientation changes, and on-screen keyboard behavior still require device smoke testing before release.

Model catalog refresh status is provider-specific and moves through not refreshed, refreshing, updated, or refresh failed text. A failed refresh keeps the previously loaded or built-in catalog and does not expose credential data. The status is session-only because dynamic model catalogs are not persisted across launches.

Fixture tests do not replace a real provider smoke test. Before release, perform one explicitly approved, low-cost request against each configured provider to confirm current production response envelopes, model identity, usage reporting, and cost presentation. Release signing, tagging, and publication are outside this phase.
