# macOS development

Unbound on macOS Apple Silicon has feature parity with Windows, using the same
shared controller, pane catalogue, controls, layouts, settings and snapping policy.
On both platforms, install once, open Resolve normally and use Unbound's pane
controls. Remaining signing/notarization work concerns distribution, not features.

## Current status

Tested configuration: Apple M2, macOS 27.0.1, DaVinci Resolve Free 21.1.1.10,
Qt 5.15.2.

| Area | Status |
| --- | --- |
| Lua startup loading | Working in Free Resolve |
| Automatic activation | Working across normal Resolve restarts |
| Pane controls and layouts | Undocking, save/recall and persistent context-menu interaction validated live |
| Fusion compatibility | Basic Inspector text input and normal interaction validated live |
| Shared/native fixtures | Passing, including settings migration, snapping, geometry and loader tests |
| Distribution | Native installer with install/update/uninstall; ad-hoc signed, Developer ID signing/notarization pending |

Shared-policy consolidation passes fixtures. See
[native tests](../../tests/platforms/macos/README.md) for repeatable checks.

## Architecture and loading

Platform-independent behavior lives once in `src/core/`. macOS supplies native
windows/events, placement, coordinate conversion and its settings-directory
lookup through `src/platform/` contracts. The loader owns no separate settings,
layout behavior or UI. See [ownership boundaries](../architecture.md).

Lua loading lives in `src/platforms/macos/loading/lua/`, with matching
`infrastructure/macos/lua/` and `tests/platforms/macos/loading/lua/` directories.

The Studio-only IO plug-in experiment was removed in favor of automatic Lua startup, which also works in Free Resolve.

The Lua approach uses Resolve's internal FFI to load a small native adapter, check
the existing Qt version and queue the shared controller onto the GUI thread.
It retains native libraries and prevents duplicate attachment. The root
`Unbound.scriptlib` invokes the private `Runtime/load.lua` once and returns silently.
It has no persistent Lua loop, project node or additional control window.
The internal Lua environment exposes global `ffi` and `os.getenv`; launcher code
must not depend on `require`, `package`, `debug` or `io`.

The loader uses Resolve's existing Qt libraries without modifying its binary
or signature. Loaded helpers remain in the process until exit, so replacing one
requires a normal Resolve restart.

## Development and installation

Requires an arm64 Mac, Xcode command-line tools, CMake, Git and Resolve. Prepare
pinned Qt 5.15.2 headers with `infrastructure/macos/prepare-qt.sh`; no replacement
Qt runtime is built or shipped. `UNBOUND_QT_SOURCE` and `UNBOUND_RESOLVE_APP`
override local toolchain paths.

Run `infrastructure/macos/build.sh` to build, test, audit dependencies and sign
an immutable internal candidate under `release/mac/lua/yyyy.mm.dd.NNN/`.
Builds do not change VERSION/tags or create ZIPs. The top-level
`infrastructure/macos/{build,test,audit,install}.sh` commands delegate to Lua.
See [installation and updates](../../infrastructure/macos/lua/README.md).

A self-contained, version-named **Unbound** app provides Install/Update and Uninstall
for the current user. Uninstall always keeps settings and layouts and offers no
option to delete them. The app requires Resolve to be closed and never terminates
it automatically. See [installer build and usage](../../infrastructure/macos/installer/README.md).
Install/update/uninstall and live installation-state detection are validated locally.

Lua installs its private loader and libraries under
`~/Library/Application Support/Unbound/Runtime/`. Only the automatic startup hook
is placed in Resolve's Fusion Scripts root; no Scripts-menu entry is installed.
Updates migrate the previous Utility-menu layout into backups outside Resolve's
Scripts tree. Settings are preserved. Startup from the private runtime folder is
verified.
Diagnostics use `~/Library/Logs/Unbound/lua-bootstrap.log`.
Fixture output stays under `build/macos/`.

## Next steps

- Complete browser-download testing;
  decide on Developer ID signing/notarization for distribution.
- Continue regression checks for supported panes/viewers, layouts, snapping,
  playback, page switching and session restoration as shared code evolves.
- Validate multiple displays, Retina scaling, display removal and Spaces/fullscreen.
- Validate broader Fusion workflows.
- Rerun native Windows checks for shared changes.

Cocoa uses logical points for geometry and shared snapping policy; native event
hit testing translates its bottom-left coordinates. Native placement/fullscreen
eligibility and window activation remain macOS responsibilities. These boundaries
must be preserved while extending platform validation.
Main-window gestures and floating-pane resizing observe native frame changes,
with shared snapping corrections applied separately. Resolve owns movement of its
integrated main-window title bar; the backend disables Cocoa's competing native
drag while attached and restores its original setting on detach. Ordinary macOS
title bars retain native dragging. Cocoa owns resize
cursor/corner hit testing; moving edges come from its frame changes, avoiding a
second corner test with different boundaries. Floating-pane header dragging uses
the pointer directly. Content layout stays deferred through native live resizing
and settles on completion or cancellation.
Main-window snap completion commits after both Cocoa and Resolve's queued Qt
release handling. The latter can restore an unsnapped position; match that
gesture's release timestamp so unrelated input and placement remain unaffected.

Main-window placement captures and restores native NSWindow rectangles, including
Resolve's integrated title area. It bypasses Qt's assumed external title-bar
clearance, preserving screen-edge placement across pages, startup and layouts.
Settings fields and monitor-recovery policy remain shared; Cocoa handles native
frame/content conversion and zoom state. Fullscreen/Spaces transitions remain
excluded from saved normal placement.
