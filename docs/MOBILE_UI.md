# Mobile UI Documentation

## Navigation

The desktop app used left, center, and right splitters. The Android app uses a top app bar, a meeting table drawer, a primary session workspace, and tabs for secondary content.

Phones use a vertical layout. Tablets and wide landscape screens use a horizontal split between the meeting workspace and transcript area.

## Main Session Screen

Desktop layout:
- Left table list.
- Center status cards and custom meeting table widget.
- Right transcript, artifacts, and log panes.
- Bottom composer in the right pane.

Mobile layout:
- Top app bar with table drawer and settings.
- Status pane with phase, round, token, cost, elapsed time, and artifact count.
- Touch-first meeting table visualization.
- Run, pause, continue, and stop buttons under the table.
- Transcript, artifacts, and log tabs.
- Composer pinned at the bottom with attachment and send actions.

UX rationale:
- Important session state stays visible in portrait.
- Secondary panes become tabs, avoiding cramped sidebars.
- Primary actions remain near thumb reach.

## Meeting Table

Desktop layout:
- Custom painted QWidget with seats around a table.
- Mouse hover tooltips and click-to-edit behavior.

Mobile layout:
- QML seat buttons arranged around a table.
- Each seat has a large tap target and shows name plus model or Empty.
- Active, decision-maker, occupied, and empty states use distinct colors.
- Tapping a seat opens the seat editor.

UX rationale:
- Hover behavior is removed because touch devices do not have reliable hover.
- Seat editing is direct and touch-sized.

## Table Management

Desktop layout:
- Persistent left sidebar with context menu actions.

Mobile layout:
- Drawer with search, list, create, duplicate, pin, rename, and delete controls.

UX rationale:
- Keeps table navigation accessible without permanently consuming phone width.
- Replaces context menus with visible actions.

## Seat Editor

Desktop layout:
- Modal dialog with form rows.

Mobile layout:
- Modal mobile dialog with occupied switch, name, provider, model, effort, and role controls.

UX rationale:
- Uses familiar mobile controls.
- Preserves validation and pending-seat behavior from the desktop version.

## Settings

Desktop layout:
- Tabbed dialog for API keys, hard stops, and visuals.

Mobile layout:
- Scrollable settings dialog with provider credentials, model refresh, and global hard stops.

UX rationale:
- Keeps high-frequency setup options accessible.
- Android Keystore is used for persisted credentials.

## Attachments

Desktop layout:
- Desktop file picker and persistent local file paths.

Mobile layout:
- Android file dialog or document picker.
- Selected files are copied into app-private storage before hashing and provider upload.

UX rationale:
- Android content URIs are not stable desktop paths, so importing protects future access.

## Accessibility And Responsiveness

- Controls use Qt Quick Controls with scalable text and large touch targets.
- Wide layouts switch to a split view for tablets and landscape.
- Text wraps in transcript cards and settings fields.
- Buttons use direct command labels and accessible names where the visible text is symbolic.
