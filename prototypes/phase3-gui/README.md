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

### C. Meeting Table

A spatial workspace that treats seats and the shared table as the product’s organizing idea. Agenda, evidence, transcript, decisions, and actions sit around a practical central meeting surface.

## Comparison matrix

| Concept | Information architecture | Visual character | Density | Navigation | Strongest use case | Likely QML complexity |
|---|---|---|---|---|---|---|
| Calm Workspace | Guided meeting steps | Quiet, editorial | Low to medium | Bottom steps and focused panels | First use and review | Low |
| Live Session | Transcript-first operations | Precise, utilitarian | High | Compact rail and workspace tabs | Active, frequent sessions | Medium |
| Meeting Table | Shared spatial work surface | Distinctive, grounded | Medium | Table regions and drawer | Branded collaboration | Medium to high |

## Accessibility and responsive behavior

All concepts use semantic landmarks, labelled native controls, keyboard-visible focus, touch-sized buttons, status text alongside color, reduced-motion rules, and close or back actions for dialogs. They are designed for 360 × 800, 412 × 915, 800 × 1280, and 1280 × 800. Transcript areas scroll independently and long labels intentionally wrap, truncate, or reveal their full text.

## Limitations

All data and transitions are demonstrative. Attachment import, provider calls, persistence, session timing, and generation are simulated locally. The prototypes are not a QML specification and do not replace production validation.

## Likely QML implementation notes

Calm Workspace maps directly to a mobile stack and sheets. Live Session needs careful split-pane and scroll-state work. Meeting Table needs the most responsive seat positioning and region priorities, but uses ordinary Qt Quick items rather than canvas or game-like rendering.

## User decisions needed

Choose the direction to take into QML: guided simplicity, live-session density, or the branded meeting-table workspace. Clarify whether the selected direction should retain the current four-tab structure or adopt its prototype navigation model.
