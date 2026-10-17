# Windows development

Supported build toolchain: Windows x64, Visual Studio 2022 C++ tools and Windows SDK,
CMake, PowerShell and Qt 5.15.2 MSVC x64 development files.

From the repository root, the existing entry point is unchanged:

```powershell
.\build-unbound-release.cmd
```

The public semantic version comes from the root `VERSION` file; override it with
`-Version 0.2.0-dev.1`. `-QtPath` overrides the default
`%USERPROFILE%\Qt\5.15.2\msvc2019_64`. The command configures the
root CMake project, builds under `build/windows/unbound/<version>/`, runs launcher
and pane fixtures, audits dependencies and publishes only `Unbound.exe` and
`Unbound.dll` under `release/win/<version>/`. Existing releases are never overwritten.
Pane fixture builds use `build/windows/pane-tests/`.

EXE and DLL file/product version strings contain the complete semantic version.
Their numeric Windows resources map `MAJOR.MINOR.PATCH` to `MAJOR,MINOR,PATCH,0`;
`-BuildNumber` overrides only that final numeric revision (0..65535). Each public
numeric component must also fit 0..65535. UTC build IDs remain in resource comments
and `release-manifest.json`, alongside the version, revision, dependencies and hashes.
Diagnostics use the same numeric version with a diagnostic suffix in the strings.

ZIP creation is never automatic. After committing and tagging `v<version>`, run
`infrastructure/windows/package-release.ps1` explicitly. It checks the clean tagged
source, embedded versions and manifest hashes, then writes
`release/packages/Unbound-<version>-windows-x64.zip` with one enclosing folder.
The package contains the two binaries, MIT license and a short README recording
the source tag and commit. Existing packages are never overwritten.

Source is in `src/platforms/windows/`; shared behavior is in `src/core/`.
Build/release implementation is in `infrastructure/windows/`. Both root release
wrappers remain usable from other working directories.

The Windows launcher still waits for Resolve, injects the adjacent DLL and exits.
The exported `UnboundStart` calling convention and return values are unchanged.
The export delegates to shared GUI-thread initialization. Floating hosts omit the
Windows caption while retaining native dark resize borders and shell identity.
The normal top frame is reduced to a one-physical-pixel outline, which is also
the native top resize target; side and bottom resize margins remain native.
An invisible inward grab band adds six logical pixels for edge resizing and
twelve-pixel corner targets, scaled to the window's DPI. Native child widgets at
the edge route resize presses into the same nonmodal host gesture; content farther
inside retains normal input. No overlay, added layout margin or visible border
is used. Maximized/fullscreen windows do not use the added grab zones. Child
hooks are removed when their host is disposed, including after docking/reparenting.
Windows 11 uses DWM's visible border thickness without subtracting a pixel;
side and bottom compensation stops at client content. Resolve's custom client
rectangle includes the DWM top outline, so the top compensation uses DWM's
thickness even when ClientToScreen reports no top margin. Snapping to the
work-area top places that outline outside the work area, alongside floating panes.
The main window commits its negotiated top snap after the native move loop ends
if release clamps it by the outline thickness; cancellation, maximization and
Alt bypass do not replay the snap. Captionless pane restoration preserves valid
saved client rectangles instead of Qt's generic title-bar safety margin.
Windows 10 retains its one-physical-pixel translucent-outline compensation.
Every PaneFloatingWindow registers itself as a snap participant at construction;
target discovery uses that host marker rather than pane-specific object names.
Viewers therefore participate in both directions with main and other floating
windows, using the same measured contact bounds and border compensation.
Dragging their Dock button uses the nonmodal movement path; an ordinary click
still docks. Native snapping and placement coordinate semantics are preserved.
Plain title labels and unused native header background use that same movement
path after the drag threshold. Header controls retain immediate native input;
editable/selectable text and custom control widgets are excluded. Header event
filters are scoped to recognized header subtrees; application-wide filtering
exists only for an active mouse gesture, including Escape and page-hide cleanup.

Window activation raises the main window and visible, non-minimized floating panes
as a group. The selected window keeps focus; peer order and geometry are preserved.
Modal dialogs and subsequent focus changes prevent a queued group raise. This is
event-driven and adds no polling. The native regression test is
`pane-float-smoke.exe --group-activation-native` with `QT_QPA_PLATFORM=windows`.

For development tools, see the [diagnostic build](../../tests/platforms/windows/diagnostic/README.md)
and [Inspector probe](../../tests/platforms/windows/inspector-probe/README.md).

Native windowing fixtures run through `infrastructure/windows/test-input.ps1`.
Each child process has a 30-second deadline; desktop cursor confinement is
released in the runner's finally block, including nonzero exits and timeouts.
