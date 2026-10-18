# Source and platform boundaries

Unbound has one shared Qt core and one selected native backend per build.
Windows remains the implemented release platform. macOS has a separate development
area and currently rejects application configuration explicitly.

## Shared code

`src/core/` owns discovery, pane registration, native toggle intent, reversible
reparenting, header buttons, page visibility, Nodes bounds, settings serialization,
per-page placement tracking and snap calculations. Widget work stays on Resolve's
Qt GUI thread. `startController()` queues initialization on the existing application
and retains the existing idempotency guard.

`panesnap.h` compares all visible Resolve window edges, including windows across
gaps. Each axis chooses the smallest adjustment within the existing snap radius;
ties favour the spatially nearer window, using the gap between rectangles.
Duplicate edge coordinates produce one effective snap position. Work-area targets
retain perpendicular overlap requirements and yield to window edges on equal
adjustments. The Windows adapter uses the existing drag-start target cache and
native contact bounds, visibility exclusions, resize limits and Alt bypass.
`PaneSnapTransition` eases only the correction over 32 ms when an axis enters or
leaves a snap zone. Quadratic ease-in starts gently and accelerates towards
contact; established locks track the pointer proposal exactly. Windows snap zones
span 24 pixels scaled for the moving window's DPI. A zero
correction remains distinguishable from an inactive snap axis. Floating windows
use a short Qt frame timer; the main window uses a native timer during its Windows
move/resize loop. Frames run only during an unfinished transition, never while
idle. Mouse release commits exact geometry, cancellation stops frames without
committing a new snap, and Alt clears the transition immediately. Raw pointer
proposals and snap-target selection are unchanged.

`src/platform/` describes the boundary:

- `windowintegration.h` includes the single CMake-selected `windowbackend.h`.
  The backend supplies `PaneFloatingWindow`, `MainWindowSnap`, dark-caption and
  window-identity helpers. This compile-time contract preserves the existing
  Windows classes without introducing another window ownership layer.
  `WindowGroupActivation` raises visible peers on activation without changing focus
  or introducing a recurring timer; its native implementation belongs to the backend.
- `windowplacement.h` declares native capture, restoration and eligibility hooks.
  The common placement tracker retains monitor checks, page lifecycle and Qt
  geometry fallback. Native Windows placement records and coordinate conventions
  are unchanged.

Core must not include platform paths or native OS headers. Backend implementation
headers can include core headers and OS APIs. Each target gets exactly one backend
include directory; there is no runtime backend selection.

## Platform implementations

`src/platforms/windows/` contains the existing launcher, DLL export, native window
classes, placement hooks and resource templates. Its `windowing.cmake` supplies the
same backend to production, shared fixtures and Windows diagnostics. The launcher
remains independent of Qt; the helper uses Resolve's Qt runtime.

`src/platforms/macos/` is reserved for Cocoa/Objective-C++ implementation and an
independently validated loading mechanism. It must implement the contracts before
building the shared controller. Further platforms should get their own source,
infrastructure, tests and documentation directories.

## Build and release infrastructure

The root `CMakeLists.txt` selects the platform. Windows resources and native link
libraries are declared only under `src/platforms/windows/`.

Windows build entry points live under `infrastructure/windows/`; platform
build scripts do not live in the repository root.
Each script resolves paths from its own location, not the caller's directory.

Build output is ignored under `build/<platform>/`. Windows release artifacts remain
exactly `Unbound.exe` and `Unbound.dll` in immutable `release/win/<version>/`
directories. Future macOS releases belong under `release/mac/`; no macOS package
or signing policy is implemented yet.

Original artwork and shared PNGs live in `src/assets/`. The Windows ICO and resource
templates live in `src/platforms/windows/resources/`. Source and artwork are covered
by the root MIT `LICENSE`. Local inspection tools are not release dependencies.

## Panel presets

`pagepresetmenu.h` uses a Qt `QFrame` tool popout with ordinary push buttons
and vector save/trash icons. It deliberately stays outside Qt's transient popup
stack: native pane docking can dismiss that stack, which previously required
hiding/reopening our QMenu and could flash. An application event filter handles
outside interaction (mouse buttons, wheel/scroll, dragging, context menus,
keyboard/text input, touch/tablet and gestures), Escape and application deactivation.
Hover alone does not dismiss it. Native user move/resize starts notify the filter
through WindowInteractionEvent, including border presses consumed by the backend.
Programmatic geometry and activation during recall are not user interaction. It exists
only while the popout is visible; there is no polling or reopen timer. The owned
tool window is frameless and stays above Resolve while open.

The anchor is the invoking button's bottom-left corner, clamped to the display
work area. The translucent popout uses the main window's Window palette role,
a 4 px corner radius, and 3 px button highlights. Dirty updates have a
playhead-style red background and a light save icon. Preset rows separate their
controls with 5 px gaps and 2 px top/bottom margins. Rows are keyed by stable preset
ID and retained across recall; active styling and dirty controls update in
place under one paint transaction. Deferred refresh requests coalesce. The
popout belongs to the stable main window, so docking its invoking pane cannot
destroy it.

`PaneSettings` format 7 stores five `presets` per page, a generated name counter,
and an optional `activePreset` UUID. Every preset has an immutable UUID and an
independent display name; operations never resolve display labels to identities. Snapshots change only on explicit update.
Active identity survives manual layout changes; direct configuration comparison
shows a dirty update control. Opening the menu flushes geometry, without polling. Direct JSON object equality detects duplicates; no hashes are
stored. Formats 1-6 migrate through the existing atomic writer and preserve their
active layouts. Preset validation prevents malformed snapshots from clearing a
page's active panes. Version 6 name-based presets receive IDs atomically,
including conversion of the active reference; snapshots and working layouts
remain unchanged. Version 7 validation requires unique IDs, not unique names.

`PresetNameButton` runs a single-shot precise 1000 ms timer only while pressed.
Movement beyond the drag threshold, leaving the button, release or hiding
cancels it. A completed hold replaces the label with an inline `QLineEdit` and
suppresses recall on release. Opening the editor explicitly activates its tool
window before focusing/selecting the text; QWidget::setFocus on an inactive tool
alone does not transfer native keyboard input. Unmodified Left/Right collapse a
selection to its start/end, with modified arrows handled by QLineEdit.
Return/Enter submits only a rename; Escape and
focus loss cancel it. Rows, callbacks and active state remain keyed by ID, so
identical labels do not conflate recall/update/delete. Editor callbacks are
cleared before widget teardown.

Dock all clears this page's floating intent (including pending lazy panes/viewers),
docks active hosts through their normal paths, and clears active preset identity.
It preserves archives, other pages and the main window placement. Its menu action
and widget are hidden and disabled without a current-page floating host; the
controller queries existing sessions/viewers, not saved pending intent or a scan.

Controller captures pending geometry/toggles before saving or deciding whether
recall is needed. Clean active-preset recall returns without applying placement,
toggles, viewer restoration or discovery; the popup skips its refresh too. Dirty
active-preset recall still restores the saved snapshot. Recall docks only
state differences before selecting the snapshot, avoiding outgoing dock callbacks
overwriting incoming geometry. Existing hosts restore geometry in place. New
floats go through `discover()`/`undock(..., true)`, lazy native initialization and
the existing toggle-recovery paths. Native toggle restoration runs off-first,
then on, once float initialization completes. Main placement reloads through its
normal page restoration path after discarding its outgoing cache. Edit viewers
reuse their startup intent and geometry restoration, including Source availability.

`UnboundPresetTests` exercises migrated settings, naming, duplicate detection,
capacity/delete/save, active identity, dirty updates, failed writes, page isolation,
button anchoring, Dock all, dirty-discard semantics and popup persistence. Its
controller fixture checks main placement, retained hosts, changed docking, native
toggles, and header entry points on Edit/Color/Fairlight. The viewer lifecycle
fixture checks both viewer buttons and preset restoration. Fixtures use temporary
settings. `--preview` opens an isolated menu preview with automatic shutdown.

## Edit viewers

Edit viewers use `editviewers.h` for native widget reparenting and independent
settings recall. `viewergeometryadapter.h` retains native region baselines while
expanding the timeline when no visible viewer remains docked. During resizing it
coalesces temporary native half-height timeline resize notifications before Qt
dispatches them to child layouts, preserving the last delivered oldSize. A narrow
application filter is installed only during reclamation and acts only on retained
resize targets. It finishes each native resize pass through the retained ancestor
chain up to the main window once, then applies the expanded regions synchronously.
Main-region painting is deferred until that transaction completes; floating
viewers retain their own updates. Docking restores native region geometry;
private Resolve layout anchors and the custom video/audio divider are not modified.

In Single Viewer Mode, native Media Pool hover preview hides the Program widget
and shows Source. A floating Program remains independently visible: a coalesced
GUI-thread update shows its existing content after the native swap completes.
The native Source preview and viewer mode remain unchanged. Native Program
visibility intent is retained for docking; Source hiding marks the return to
Program even when Resolve's own show call is already satisfied. Ownership changes
and Unbound's own show calls are excluded from intent tracking. Page/host hiding
still wins; no frames are copied and no playback or private native handler is used.

## Tests

`tests/core/pane-float/` contains shared Qt fixtures. Its platform test contract
isolates native bootstrap invocation, window placement checks and native window
tests in `tests/platforms/windows/windowing/`. The current fixture executable uses
the Windows backend; a Mac backend and test adapter are still required to run it
on macOS. No fixture coverage was intentionally removed during the split.

Launcher tests, Inspector probes and the preserved diagnostic controller are under
`tests/platforms/windows/`. The release script retains its existing test invocations
and dependency audit. Native interactive tests remain manually selectable with
`--snap-native` and `--taskbar-native` on Windows.

A successful structural/configuration check on macOS does not validate Windows
compilation or runtime behavior. Run the Windows release command on its supported
toolchain and validate a newly built helper in a fresh Resolve process.

Shared pane metadata lives in `src/core/panecatalogue.h`; discovery, header
anchors and settings migration consume it. Shared source lists live in
`src/core/CMakeLists.txt`, and fixture targets in `tests/core/pane-float/targets.cmake`.
Native fixture operations use `layoutadapter.h` with platform implementations.
Settings schema and atomic writes remain shared; `src/platform/settingslocation.h`
selects only the native data directory. Appearance, snap constants and reachable
geometry policy are shared, with existing Windows native behavior preserved.
