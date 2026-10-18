# macOS native tests

Build through root CMake or `infrastructure/macos/build.sh`. Run
`infrastructure/macos/test.sh` to exercise the built artifacts. Tests create
short-lived Cocoa windows and isolated temporary settings; they do not manipulate
an editing session. Resolve's installed Cocoa platform plugin is required.

`platformtests.mm` adapts the shared pane fixtures and checks captionless Cocoa
keyboard focus, gestures/cancellation, programmatic geometry isolation, and the
shared layout popout's save/update/rename/focus/dismissal behavior.
The layout fixture delivers Cocoa right-button events only to its own NSView,
covering press-triggered menu opening, release over the invoking button, small
held movement, repeated opening and later outside click/drag dismissal. It also
checks that opening with no held button preserves ordinary release dismissal.

`layoutadapter.mm` runs `tests/core/pane-float/layouttest.cpp` and its catalogue
checks as `UnboundLayoutTests`, using Cocoa placement, keyboard focus and native
resize events. The shared suite covers settings migration, layout identity,
controller-driven recall, retained hosts/rows, docking and page isolation.

`--mac-main-resize-test` exercises shrinking from all main-window edges and corners
through the native event monitor. Cocoa frame proposals precede Qt geometry/pointer
updates; opposite edges and the committed size must remain stable after release.
The fixture uses its own window without moving the physical cursor or touching Resolve.

`--mac-floating-resize-test` exercises all pane edges/corners, including asymmetric
corner insets that the old narrow border test classified as a single edge. It
checks axes that begin moving separately, fixed opposite edges, content layout
deferral, release stability, two-axis snapping and explicit cancellation using
native resize notifications and frame proposals.

`--mac-main-snap-test` models Cocoa's final frame delivery and Resolve's later
Qt mouse-release handler restoring an unsnapped position. It covers work-area
and floating-peer edges, release during/after the snap transition, repeated
drags, and isolation from newer gestures, unrelated releases and placement changes.

`--mac-main-drag-test` repeats main snapping/release and all-edge resize checks
with Resolve's integrated title style. It checks exclusive host movement, retained
native resize/title controls, repeated discovery, and restoration on detach.

`--mac-placement-test` verifies native capture/restore with ordinary and integrated
title bars, repeated screen-edge placement, page switching, startup and layout
recall, maximized/normal frames, legacy Windows fallback, monitor recovery and
content size constraints. The shared `--main-placement-test` also checks special
mode exclusions, including fullscreen and dual-screen layouts.

`infrastructure/macos/lua/test.sh` runs the Lua native loader fixtures
and the same shared/Cocoa suite. `loading/lua/loader.cpp` checks missing Qt,
worker-thread entry, main-thread attachment, retained callbacks, repeated starts
and an existing controller. `loading/lua/probe.lua` is a read-only live console
probe built by `infrastructure/macos/lua/probe.sh`; pass the repository path as
printed by that script. It does not depend on Lua's unavailable `debug` library.
Lua live checks use `~/Library/Logs/Unbound/lua-bootstrap.log` and must distinguish
request acceptance from completed attachment and actual docking-button discovery.
`loading/lua/startup/` tests the startup hook and private loader using
Resolve's LuaJIT in a separate fixture process. Mocked native calls verify private installation paths and silent success;
restricted Lua globals, missing files/FFI and rejected attachment do not leave
an exception escaping the startup hook. Current live-validation status belongs in
[platform status](../../../doc/platforms/macos.md); individual runs belong in logs.

`loading/lua/install_test.py` exercises fresh installs, legacy menu migration,
updates, disable/enable, rejected installs and rollback in isolated user roots.
OS signature/process checks are mocked; candidate builds also verify real signatures.

The adapter selects Fusion for synthetic widgets: the macOS native style's layout
margins differ from the shared fixture's expected sizing. It waits for real Cocoa
fullscreen completion notifications/state rather than assuming a single Qt event
pass finishes a Space transition. No production Qt style is changed.

Live checklist for a fresh Resolve instance (Free or Studio):

1. Launch normally and confirm no Unbound entry appears in Workspace → Scripts.
   Confirm this PID's load, GUI-thread validation, controller attachment and first
   shared docking control in `~/Library/Logs/Unbound/lua-bootstrap.log`.
2. Open a disposable project. Verify the existing overlapping-squares controls;
   undock/redock a pane, use its native controls, close it to dock, and verify its
   native toolbar toggle reclaims/restores space.
3. Check header/button drag, all edges, Option snap bypass, native child surfaces,
   page switching, independent viewers and activation focus during playback.
4. Right-click the existing button; save/recall/update/rename/delete a layout,
   verify persistent popout and outside/native-gesture dismissal.
5. Check Fusion Inspector controls/text input after automatic startup; controls
   must remain usable and unduplicated.
6. Restart normally; verify pane/toggle/main placement restoration. Repeat with
   multiple displays, Retina scaling, display removal and Spaces/fullscreen.

A compile or synthetic fixture pass does not certify the live checklist. Windows
release tests are run on Windows; they cannot be certified by this Mac build.
