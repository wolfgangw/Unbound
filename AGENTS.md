# Unbound development

## Product

Unbound makes DaVinci Resolve panes independent, movable, resizable windows on
Windows x64. Edit supports Media Pool, Effects, Sound Library, Keyframe Editor,
Edit Index, Inspector, Metadata and Mixer; Color supports Nodes, Gallery, LUTs
and Media Pool. Floating windows and the main window snap to one another and
display work-area edges during movement/resizing; Alt bypasses snapping.
Window edges also align across gaps. The smallest edge adjustment wins; equal
adjustments prefer the nearer window. Work-area edge snapping stays local.
Snap zones are 24 pixels scaled for DPI. Capture/release uses a short 32 ms
quadratic ease-in (accelerating towards contact); pointer movement remains
direct, release commits exact geometry, and frame timers run only in transitions.
Right-click any Unbound button for up to five panel configuration layouts per
page. Save/recall/delete keep the popup open; outside interactions dismiss it,
including scrolling, keyboard input and dragging. Hover alone leaves it open. Names are
generated per page; identical stored configurations do not create duplicates.
Layouts include main placement, supported panes' floating geometry/docking and
native toggle states, with existing restoration/monitor recovery. They do not
change Resolve's viewer mode or internal panel split positions. Settings format
7 migrates previous working arrangements without creating saved layouts. Existing name-based
layouts receive stable UUIDs and an active-ID reference without changing layouts. The active layout stays
highlighted; when dirty it offers an update icon left of trash. Only explicit
updates overwrite snapshots. Save names the current page, e.g. "Save as new Edit layout",
using the shared page catalogue's capitalized label. The popup
anchors below the invoking button. Dock all affects only the current page,
clears pending undocking/active layout identity and preserves saved layouts.
Dock all is hidden/disabled without an actual floating host on the current page,
including viewers. The popup inherits the main-window background palette and has
4 px rounded corners; highlights use 3 px corners. Dirty update buttons use playhead-style
red; trash stays neutral. Layout row controls have 5 px horizontal gaps and
2 px top/bottom margins. Clean active-layout recall is a no-op after a fresh
state capture; dirty recall restores the saved snapshot. Retain layout rows and
coalesce updates rather than rebuilding rows on recall. The persistent popout
uses a Qt tool QFrame rather than Qt's transient popup stack, so native docking
cannot dismiss/reopen it. Outside clicks, Escape and application deactivation
hide it; its application filter exists only while visible.
Native user move/resize starts also dismiss it, including native-child resize
edges. Backends report WindowInteractionEvent before starting those gestures;
programmatic geometry changes during recall do not emit it. Switching layouts
discards the outgoing unsaved changes; there are no per-layout drafts.
Hold a layout name for 1 second to edit inline. Enter renames only, even for an
inactive layout; Escape or focus loss cancels. Activate the popout when opening
the editor. While it owns focus, hand off only that popout's key/shortcut events
directly to the owned editor's Qt event handler. Resolve can swallow key presses
even with correct native/Qt focus; sendEvent re-enters the same host routing.
Do not synthesize clicks or alter Resolve's global shortcut handling. Left/Right without
modifiers collapse selection to its start/end. Names may repeat (up to 128
characters, nonblank). Recall/update/delete and retained rows use stable IDs,
never display names. Rename preserves active identity and dirty/layout state.
Use layout terminology in UI, code and tests. Retain format-7 JSON wire keys
`presets`, `activePreset` and `nextPresetNumber` for compatibility, and preserve
Resolve-owned widget names such as `btnPresetsSelector`.

Fairlight supports Media Pool, Effects, Index, Groups, Sound Library, ADR, Mixer,
Metadata and Inspector; Meters is excluded. Fairlight docking controls must sit
immediately left of the native options action (Create Group for Groups), never
in a fallback strip. Fairlight settings belong to `WStackPage_Audio`.

The overlapping-squares header button toggles docking on click and moves an
already-floating pane on drag. Floating windows have no Windows title bar. They
can also be moved by dragging plain title text or unused native header background;
native controls and selectable text are excluded from those drag surfaces. They
retain native resize borders, taskbar/Alt+Tab identity and window commands.
Closing a floating window also docks it. Strokes stay muted grey (#848484) in
either state and brighten only on hover. Tooltips are "Undock" and
"Click to dock; drag to move". Manual undocking turns off the
native Resolve pane toggle to reclaim main-window space; docking turns it on
to show the restored pane, deferred until page return if inactive. Native toggles
remain usable, and startup restoration honors saved choices. Remember geometry, docking and native toggle
states across sessions; hide/show retained floating windows with their page.
Main-window placement is remembered per page.
Edit's Program and Source viewers use normal header buttons, independent saved
geometry/floating state and native Single/Dual Viewer controls, without an extra
control window. Source restoration waits for native availability. An event-driven
geometry adapter reclaims the viewer region when no visible viewer remains docked.
Unbound can start first and wait
silently for Resolve, without launching it.
Activating the main window or a floating pane raises the visible, non-minimized
group while preserving selected-window focus. Group activation is event-driven.

## Architecture

- `src/platforms/windows/launcher.cpp`: Win32 launcher locates a ready Resolve process, loads the
  adjacent `Unbound.dll` into it and invokes its bootstrap. The launcher then
  exits; the helper lives inside Resolve. Waiting launchers use a single-instance
  guard. No network service, temporary DLL extraction or autostart registration.
- `src/core/controller.cpp`: Qt GUI-thread discovery by widget object names, native
  initialization, pane registration, toggle tracking and deferred page restoration.
- `sidebarfloatsession.h`, `panefloatsession.h`: reparent existing widgets into
  floating hosts while preserving original layout slots, constraints and ownership.
  Docking restores them. Resolve owns content; Unbound owns hosts/placeholders.
- `panedecoration.h`: inserts the vector toggle beside recognized native header
  menus, with a compact fallback strip when necessary.
- `panefloatingwindow.h`, `panetaskbar.h`: captionless hosts, native dark borders,
  taskbar identity, page visibility and mouse handling. Nonmodal drag handling and
  deferred content layout during resizing preserve playback responsiveness.
- `panesnap*.h`, `mainwindowsnap.h`: native window/contact bounds, display work
  areas, snapping and resize constraints. `nodescanvasbounds.h` maintains Nodes
  scene bounds as its viewport changes.
- `panesettings.h`, `geometryjson.h`, `mainwindowplacement.h`: readable JSON,
  atomic saves, geometry conversion, per-page placement and monitor checks.
  Settings: `%LOCALAPPDATA%\Unbound\Unbound.json`; `UNBOUND_SETTINGS_FILE` overrides
  the path for tests. Page/pane keys are actual Resolve widget names. Qt geometry
  records exist only in memory; persisted fields are unencoded and named.

`src/core/panecatalogue.h` owns page/pane metadata for discovery, native toggles,
header anchors and persistence/migration. Add pane metadata there once; specialized
layout behavior stays in shared core. `src/core/CMakeLists.txt` owns the shared
production source list. Both platform helpers and controller fixtures use
`unbound_link_core()`. Shared fixture targets live in
`tests/core/pane-float/targets.cmake`; native layout operations implement
`layoutadapter.h` under the platform's tests directory.

Shared pane behavior and persistence live in `src/core/`. OS contracts live in
`src/platform/`; CMake selects the implementation under `src/platforms/<platform>/`.
Keep native APIs out of shared code. Windows bootstrap is in
`src/platforms/windows/bootstrap.cpp`; native placement is in that directory's
`windowplacement.cpp`, while per-page tracking remains in core.
Platform separation must never create independent versions of settings, layouts,
pane support, visual design or snapping policy. Those mechanisms belong in shared
core and must retain identical semantics. Settings directory discovery alone uses
`src/platform/settingslocation.h`; filenames, overrides, JSON schema, migration
and atomic writes remain shared. `panewindowappearance.h`, `panesnap.h` and
`panegeometryrestore.h` own host colors, snap constants/calculations and reachable
captionless geometry recovery. Native backends supply coordinate/DPI conversion,
OS events, native placement and window rendering. Reuse shared fixtures on each
backend; keep native event fixtures under that platform. Preserve working Windows
behavior when extracting shared implementation; do not replace its backend.
macOS main placement uses native NSWindow capture/apply with the shared geometry
fields and recovery policy. Do not restore it through QWidget::restoreGeometry:
Qt assumes external title-bar clearance that Resolve's integrated frame lacks.
Windows build entry points live under `infrastructure/windows/`; macOS entry
points live under `infrastructure/macos/`. Keep platform build scripts out of the
repository root.
macOS source, infrastructure and tests have separate development areas. The
experimental Cocoa backend loads the unchanged shared controller through
`loading/lua/`, with matching infrastructure and loader-test directories.
Keep the existing UI concept and functions.
Do not introduce a separate workspace/control window. Do not substitute a no-op backend for a release.
See `doc/architecture.md` and `doc/platforms/` for platform status.

## Build and validate

Run from this repository root:

```powershell
.\infrastructure\windows\build-release.cmd
# Override the default %USERPROFILE%\Qt\5.15.2\msvc2019_64 when needed:
.\infrastructure\windows\build-release.cmd -QtPath C:\Qt\5.15.2\msvc2019_64
```

Requires Windows x64, VS2022 C++ tools/Windows SDK, CMake, PowerShell and Qt 5.15.2
MSVC x64 development files. Runtime uses Resolve's Qt libraries. The script builds
both binaries, runs `tests/platforms/windows/launcher` and
`tests/core/pane-float` with the Windows test adapter, and audits dependencies.
Successful releases contain only `Unbound.exe` and `Unbound.dll` in
`release/win/<version>/`. Never overwrite published version directories.
The default semantic version is read from root `VERSION`; `-Version` overrides it.
`-BuildNumber` sets the fourth numeric Windows revision (default 0), not the public
version. Build/release output is ignored
by Git. The release build regenerates icons from `src/assets/unbound-symbol.svg`;
shared artwork is in `src/assets`; the ICO is in
`src/platforms/windows/resources`. Build output is in `build/windows/`.
Create distribution ZIP packages only when explicitly requested, never
automatically as part of a build.
Feature development normally requires an internal build in
`release/win/<yyyy.mm.dd.daily_counter>/` (counter padded to three digits).
Use `-InternalBuild`; keep VERSION/public semantic versions unchanged. Internal
builds do not create ZIP packages or tag HEAD. Publishing a proper release,
changing its public version/tag or packaging requires an explicit request.

Mac development uses Resolve's Qt 5.15.2 runtime and pinned headers prepared by
`infrastructure/macos/prepare-qt.sh`. Build/test/audit/sign with
`infrastructure/macos/build.sh` (delegates to `lua/build.sh`).
Candidates are immutable under `release/mac/lua/`.
Top-level Mac build/test/audit/install commands delegate to Lua. Native Cocoa
fixtures require a GUI session; Windows native validation still requires Windows.
No Resolve app re-signing, public version change or automatic ZIP.
The native installer UI lives in `src/platforms/macos/installer/` and delegates to
Lua install/uninstall scripts. Package it explicitly with
`infrastructure/macos/installer/build.sh <Lua-candidate>`; its app is self-contained
and has no persistent service. Uninstall retains settings unless the user explicitly
chooses their removal. Both actions
require Resolve closed and must not terminate it automatically.

Lua loading and automatic startup work in the tested Free Resolve
configuration; pane undocking, layouts and basic Fusion interaction are validated.
See `doc/platforms/macos.md` for current limits and next steps, and the approach's
infrastructure README for installation/recovery commands.

The Lua startup hook calls the private loader once and returns silently. Install
the runtime under `~/Library/Application Support/Unbound/Runtime/`; only the root
`Unbound.scriptlib` belongs in Resolve's Scripts tree. Do not install a visible
Scripts-menu launcher. Normal installation includes startup and migrates the old
Utility launcher into backups outside Resolve's scan tree, preserving settings. Keep
its source/infrastructure/tests under the Lua approach's `startup/` directories.
Do not add a persistent Lua loop or duplicate controller/settings logic. Resolve's
internal Lua exposes global `ffi` and `os.getenv`, but omits `require`, `package`,
`debug` and `io`. Queue native attachment onto the Qt GUI thread. Keep fixture
logs separate from live startup logs using `UNBOUND_LOG_FILE`.

## Development constraints

- Keep widget/layout manipulation on Resolve's Qt GUI thread. Preserve native
  signals, ownership, user state, page context and reversible docking.
- Initialize native pane content before automatic detachment; some widgets are
  lazily populated. Restore saved native toggle states after initialization.
- Preserve native dark resize borders and immediate page switching with
  retained hosts/content. Initial restoration has a separate short fade.
- Maintain explicit coordinate-space distinctions: Qt logical client geometry,
  Windows placement coordinates and visible frame/contact bounds differ. Diagnose
  restoration errors at their source before adding positional corrections.
- Automated fixtures cannot fully model Resolve. Use a fresh Resolve instance
  to validate a new helper; already injected DLLs are not updated by replacing files.
  Do not terminate the user's Resolve session without instruction.
- Tested configuration: Resolve 21.1.0.14, Windows x64, Qt 5.15.2. Other versions
  need validation. Known issue: Media Pool may widen on startup restoration and
  overlap its neighbor. Native minimum sizes apply; Nodes may need manual panning
  after docking into a narrower viewport.
- Source and symbol artwork license is MIT. Preserve applicable third-party
  notices. GammaRay is a local inspection tool and is not shipped with Unbound.

Keep README.md and doc/Unbound settings.md accurate for current behavior. This repository
is self-contained for development apart from the installed toolchain and Resolve.
README.md is the public GitHub landing page. AGENTS.md records durable development
rules; platform docs summarize current status, meaningful limitations and roadmap;
test READMEs explain reproducible validation. Keep per-run output in build logs.
Do not append test-session diaries, PIDs, candidate-by-candidate histories or
incidental corrections to these documents. Retain investigation details only when
they explain a lasting constraint, architectural decision or unresolved defect.

## Local inspection tools

`tools/gammaray/` is ignored by Git and contains the modified JSON-export GammaRay
package, `run-gammaray.cmd`, and page snapshots. See its README for capture steps.
Edit's page widget is `WStackPage_Conform`; Color is `WStackPage_Color`, Fairlight
is `WStackPage_Audio`. Use a fresh Resolve session without Unbound for native
captures, and verify exports have `truncated: false`.
