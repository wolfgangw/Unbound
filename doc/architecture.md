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

`build-unbound-release.cmd` and `build-unbound-release.ps1` remain compatible public
entry points. Implementation is in `infrastructure/windows/`, with separate icon
and dependency-audit helpers. Windows diagnostic build commands live there too.
Each script resolves paths from its own location, not the caller's directory.

Build output is ignored under `build/<platform>/`. Windows release artifacts remain
exactly `Unbound.exe` and `Unbound.dll` in immutable `release/win/<version>/`
directories. Future macOS releases belong under `release/mac/`; no macOS package
or signing policy is implemented yet.

Original artwork and shared PNGs live in `src/assets/`. The Windows ICO and resource
templates live in `src/platforms/windows/resources/`. Source and artwork are covered
by the root MIT `LICENSE`. Local inspection tools are not release dependencies.

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
