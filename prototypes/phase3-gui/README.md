# Phase 3 GUI concepts

These are non-production HTML, CSS, and JavaScript prototypes for choosing a Phase 3 visual direction. They do not use the application database, credentials, providers, filesystem, or network.

## Open the launcher

Open `index.html` in this folder in a browser. Each concept also works when its own `index.html` is opened directly.

## Files

- `index.html`, `launcher.css`, `launcher.js`: concept launcher and comparison.
- `concept-a/`: Calm Workspace.
- `concept-b/`: Live Session.
- `concept-c/`: Meeting Table.
- `screenshots/`: browser validation captures when available.

## Workflows represented

- Empty and existing meeting tables, including creating or opening a table.
- Seat configuration with provider, model, effort, and role.
- Research, planning, execution, quality control, presentation, and completed phases.
- Live or recorded transcript, artifacts, decisions, action items, attachments, and settings.
- Attachment selection, import progress, oversize rejection, normal notifications, generation success, cancellation, provider failure, and outcome-unknown handling.

## Concepts

### A. Calm Workspace

A guided, mobile-first workspace with one primary task at a time. It is the quietest option and makes first-use setup, meeting review, and safe recovery states easy to understand.

### B. Live Session

A dense active-meeting console built around the transcript. Persistent session telemetry and fast panels favor frequent operators who need immediate visibility of speakers, generation state, attachments, and provider status.

## Preferred Concept B refinement

Concept B is the preferred structural direction. Its Session page retains transcript-first operations with Seats, Attachments, Generation, and Artifacts panels. The primary session control cycles through Start, Pause, and Continue based on the local demo session state. Artifacts open in a responsive preview dialog and can be shown as populated or empty.

The refined concept supports Light, Dark, and System appearance preferences; Signal session (Concept B) and Calm workspace (Concept A) color themes; and system, workspace, and console font styles. The selected settings persist only for the current browser session.

Transcript messages show a speaker name and role on the left, with time and model on the right. Seat colors are editable through each seat card and update the transcript accents immediately. The settings prototype represents provider credentials, model catalog refresh, actual global hard stops, attachment safeguards, and appearance. It uses fake values only and makes no provider, credential, filesystem, or network request.

### C. Meeting Table

A spatial workspace that treats seats and the shared table as the product’s organizing idea. Agenda, evidence, transcript, decisions, and actions sit around a practical central meeting surface.

## Comparison matrix

| Concept | Information architecture | Visual character | Density | Navigation | Strongest use case | Likely QML complexity |
|---|---|---|---|---|---|---|
| Calm Workspace | Guided meeting steps | Quiet, editorial | Low to medium | Bottom steps and focused panels | First use and review | Low |
| Live Session | Transcript-first operations | Precise, utilitarian | High | Compact rail and workspace tabs | Active, frequent sessions | Medium |
| Meeting Table | Shared spatial work surface | Distinctive, grounded | Medium | Table regions and drawer | Branded collaboration | Medium to high |

## Accessibility and responsive behavior

All concepts use semantic landmarks, labelled native controls, keyboard-visible focus, touch-sized buttons, status text alongside color, reduced-motion rules, and close or back actions for dialogs. The refined Concept B is designed for 360 × 800, 412 × 915, 800 × 1280, and 1280 × 800. Transcript areas scroll independently and long labels intentionally wrap, truncate, or reveal their full text.

## Limitations

All data and transitions are demonstrative. Attachment import, provider calls, persistence beyond the current browser session, session timing, and generation are simulated locally. The prototypes are not a QML specification and do not replace production validation.

## Likely QML implementation notes

Calm Workspace maps directly to a mobile stack and sheets. Live Session needs careful split-pane and scroll-state work. Meeting Table needs the most responsive seat positioning and region priorities, but uses ordinary Qt Quick items rather than canvas or game-like rendering.

## User decisions needed

Choose the direction to take into QML: guided simplicity, live-session density, or the branded meeting-table workspace. Clarify whether the selected direction should retain the current four-tab structure or adopt its prototype navigation model.
