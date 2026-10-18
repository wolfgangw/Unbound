# Windows development

Windows and macOS have feature parity: the same panels/viewers, layouts, snapping
and session restoration, with automatic startup and native installers. Windows
builds are currently unsigned; see the [README](../../README.md#compatibility)
for opening downloaded installers.

Supported build toolchain: Windows x64, Visual Studio 2022 C++ tools and Windows SDK,
CMake, PowerShell, Python 3 and Qt 5.15.2 MSVC x64 development files.

From the repository root:

```powershell
.\infrastructure\windows\build-release.cmd
```

The public semantic version comes from the root `VERSION` file; override it with
`-Version 0.2.0-dev.1`. `-QtPath` overrides the default
`%USERPROFILE%\Qt\5.15.2\msvc2019_64`. The command configures the
root CMake project, builds under `build/windows/unbound/<version>/`, runs loader,
installer and pane fixtures, audits dependencies and produces a self-contained
`Unbound <version>.exe` installer under `release/win/<version>/`.
Existing releases are never overwritten.
Pane fixture builds use `build/windows/pane-tests/`.

For feature development, run `./infrastructure/windows/build-release.cmd -InternalBuild`.
The same tests and dependency audit run, with output in
`release/win/<yyyy.mm.dd.daily_counter>/` (three-digit daily counter). `VERSION`
stays unchanged; embedded semantic version strings carry internal build metadata
and the Windows numeric revision uses the daily counter. No ZIP or tag is created.
Use a fresh Resolve session to load a rebuilt helper. Viewer integration uses
normal header controls with no separate development control panel.

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
The package contains the installer, MIT license and a short README recording
the source tag and commit. Existing packages are never overwritten.

Source is in `src/platforms/windows/`; shared behavior is in `src/core/`.
Build/release commands live in `infrastructure/windows/`: `build-release.cmd`
invokes the adjacent `build-release.ps1`, which can also be called directly.
Both resolve paths from their own location and work from other directories.

Unbound loads automatically through Resolve's Lua startup mechanism from its
per-user Scripts root, with a native Install/Update/Uninstall app. Automatic
attachment, saved Program Viewer restoration and the layout menu have been
validated in Windows Studio 21.1.1.10. Windows Free remains unvalidated.
See [installer development](../../infrastructure/windows/installer/README.md)
and [loader tests](../../tests/platforms/windows/loading/lua/README.md).
The Qt-free loader validates the host Qt version and loads the adjacent helper;
`UnboundLuaAttach` queues shared GUI-thread initialization. Floating hosts omit the
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

Cinema Viewer from a floating Edit viewer also hides the main window, matching
Resolve's docked Cinema behavior. Otherwise Windows keeps the main menu's
application shortcuts eligible alongside Cinema's copies and transport commands
can stop working. Exit restores only the visibility Unbound changed; shortcuts,
viewer parenting and geometry are untouched. See the
[Cinema routing tests](../../tests/platforms/windows/cinema-focus/README.md).

For development tools, see the [Inspector probe](../../tests/platforms/windows/inspector-probe/README.md).

Native windowing fixtures run through `infrastructure/windows/test-input.ps1`.
Each child process has a 30-second deadline; desktop cursor confinement is
released in the runner's finally block, including nonzero exits and timeouts.
