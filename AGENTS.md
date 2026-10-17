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

Shared pane behavior and persistence live in `src/core/`. OS contracts live in
`src/platform/`; CMake selects the implementation under `src/platforms/<platform>/`.
Keep native APIs out of shared code. Windows bootstrap is in
`src/platforms/windows/bootstrap.cpp`; native placement is in that directory's
`windowplacement.cpp`, while per-page tracking remains in core.
The root release entry points delegate to `infrastructure/windows/`.
macOS source, infrastructure and tests have separate development areas; the
macOS backend is not implemented. Do not substitute a no-op backend for a release.
See `doc/architecture.md` and `doc/platforms/` for platform status.

## Build and validate

Run from this repository root:

```powershell
.\build-unbound-release.cmd
# Override the default %USERPROFILE%\Qt\5.15.2\msvc2019_64 when needed:
.\build-unbound-release.cmd -QtPath C:\Qt\5.15.2\msvc2019_64
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

README.md is the public GitHub landing page. Keep internal build/testing notes
and development investigations in AGENTS.md, doc/ or the relevant tests directory.

## Local inspection tools

`tools/gammaray/` is ignored by Git and contains the modified JSON-export GammaRay
package, `run-gammaray.cmd`, and page snapshots. See its README for capture steps.
Edit's page widget is `WStackPage_Conform`; Color is `WStackPage_Color`, Fairlight
is `WStackPage_Audio`. Use a fresh Resolve session without Unbound for native
captures, and verify exports have `truncated: false`.
