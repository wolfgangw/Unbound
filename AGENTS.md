# Unbound development

## Product

Unbound makes DaVinci Resolve panes independent, movable, resizable windows on
Windows x64 and macOS, with feature parity. Universal Mac builds include arm64 and
x86_64; Intel support is offered for community testing without native Intel
validation. Keep this limitation explicit in public documentation. Both use automatic
startup, native installers and the same shared pane behavior and layouts.
Windows builds are unsigned; macOS builds are ad-hoc signed without Developer ID
signing/notarization. These distribution limitations do not imply missing features.
Tested configurations and platform-specific mechanisms are recorded below.
Edit supports Media Pool, Effects, Sound Library, Keyframe Editor,
Edit Index, Inspector, Metadata and Mixer; Color supports Nodes, Gallery, LUTs
Media Pool, Effects, Timeline and Clips. Color Timeline floats independently of Clips;
its Unbound button overlays the native timeline's top-left corner without adding
a header or changing its native docked slot constraints. The snapshot's 76 px slot
height is configuration-dependent; the transferred UiMiniTimelineWidget has no
explicit Qt maximum height. Native resizing and clip selection are validated on
Windows. The floating host has a 240 logical-pixel minimum width for track labels
and usable clip bars; native docked constraints remain unchanged.
Clips uses the native Lightbox renderer while floating, including its thumbnail
size control, filtering and timecode ruler. Its button sits centered in a reserved
32 px leading gutter, aligned with the first thumbnail decoration row in docked
and floating states. The unused gutter also drags the floating window, without
covering clip decoration or native controls. A shared event-driven adapter replaces
the empty Lightbox allocation with the main viewer, suppressing interim repaint
during native layout passes. Hosts use normal geometry/layout persistence and
page hide/show retention. Windows content reflows on resize release; the floating host minimum is 360 by
240 logical pixels. Native
Lightbox exit docks Clips; docking restores the pre-float Lightbox/Color Controls
choices, deferred if the page is inactive. Other Color panes initialize before
Clips enters Lightbox; layout recall temporarily suspends its native mode.
On startup, temporarily leave native Lightbox while other saved Color panes are
pending, including not-yet-discovered panes. Keep retained Clips suspended across
deferred discovery passes; enter its native mode only after those attempts finish.
Preserve the original Lightbox choice for docking. Use widget/layout events, not
polling or fixed delays. Native grid overflow scrolling and header-background
dragging are confirmed; leave scrolling to Resolve.
The same deferred-mode guard covers layout recall even when the incoming layout
does not float Clips: docking outgoing Clips can restore native Lightbox-on after
recall suspended it. Keep the toolbar available until incoming Color panes finish
initializing, then restore the preserved native mode.
Explicit recall of a Color layout without floating Clips returns to normal Color
mode, including reselecting an otherwise clean active layout. Do not restore the
outgoing Clips session's Lightbox-on choice after that recall. Manual docking
retains its separate pre-float mode restoration behavior; no new settings field.
While Clips floats, transfer the existing
normal Color controls into an additional row above the native Lightbox toolbar.
Insert that row into the native vertical layout and restore original control
ownership/order when docking. The shared adapter reserves the native left stack
(Gallery/LUTs/Media Pool) and right Nodes/Effects containers according to their
native toggles, alongside the viewer and optional Timeline. Preserve native
constraints and connections. While dragging a recognized sidebar divider, freeze
main-window/Clips-host repaint, temporarily enter normal Color mode and defer ordered
mouse-event delivery until its layout pass. Capture native widths before returning
to Lightbox and repaint on release. A separate nonactivating, input-transparent two-pixel grey
guide follows the native divider position while the main backing store is frozen.
Dispose of it on completion or interruption. Interrupted gestures must release native input,
discard stale queued events and restore prior repaint state. Cocoa additionally
holds a single composited image of each affected window: disabling Qt updates
alone does not freeze Resolve's native Metal surfaces, whose interim resizing
stretches the viewer, exposes blank background and squashes the floating Clips
grid. The platform presentation contract prepares these images before the shared
normal-mode/input transaction;
input received during preparation retains its order. ScreenCaptureKit discovers
only current-process windows without TCC consent (macOS 14.4+; tested Resolve
requires macOS 15). Covers pass input through, remain in memory only and are
removed after restored layout/repaint delivery, or immediately on cancellation
before preparation completes. Failed preparation leaves native mode and widths
unchanged and restores updates. Windows retains its existing Qt backing-store
freeze. No persistent capture stream, screen-access request, polling, binary
offsets or saved divider positions. Native Windows docking, layout recall, panel
visibility and repaint preservation are user-validated. macOS divider presentation
and Clips button alignment are user-validated in the tested configuration; broader
gesture/platform coverage remains in `doc/issues/platform-validation.md`.
Floating windows and the main window snap to one another and
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

Edit and Fairlight Sound Library audition players use the shared activation bridge
also used by Fairlight Media Pool. Bind only the recognized host/page/player before
first show. On genuine host activation, one main-window activation notification
restores Resolve's command context; consume that exact event before QWidget
propagation and let genuine focus select the player. Do not forward transport keys,
force focus or alter waveform drawing. Sound Library hosts need no Qt parent change.

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
After docking, a bounded shared check verifies the native allocation after layout
settles. A visible area collapsed to one pixel (including clipping by ancestors)
returns the panel to its saved floating geometry without activation. Inactive pages
defer the check until return; intentional native hiding, including unavailable
viewers, is respected. Failed docking saves the actual floating state without
overwriting saved layouts. The affected button pulses from muted grey to playhead
red until its own context menu is opened, then remains muted. Its tooltip and menu
explanation persist until successful docking; another failure restarts the pulse.
Animation runs only while the warning is unacknowledged, visible and the app is
active. The Edit Media Pool explanation suggests disabling optimized vertical UI;
other panels use general guidance. Do not change Resolve preferences or project
resolution automatically, force slot widths, or show a modal docking warning.
Main-window placement is remembered per page.
Edit's Program and Source viewers use normal header buttons, independent saved
geometry/floating state and native Single/Dual Viewer controls, without an extra
control window. Source restoration waits for native availability. An event-driven
geometry adapter reclaims the viewer region when no visible viewer remains docked.
Installed Unbound starts automatically with DaVinci Resolve through its Lua
startup hook; the installer does not launch Resolve.
Activating the main window or a floating pane raises the visible, non-minimized
group while preserving selected-window focus. Group activation is event-driven.
Page switches recall the last focused window/control per page within the running
session, falling back to the main window's native focus when unavailable. Focus
is not saved in settings or layouts. Automatic pane reveals must not activate
their windows; set WA_ShowWithoutActivating before native creation on both
backends. Explicit manual undocking still activates the new host. Shared
pagefocus.h ignores focus changes caused by page navigation and cancels deferred
recall for newer input, app deactivation or superseding page changes; dialogs
retain priority. Do not intercept transport or native page shortcuts.

## Architecture

- `src/platforms/windows/loading/lua/`: Resolve's startup hook loads the Qt-free
  `UnboundLuaLoader.dll`, which validates the host and loads adjacent `Unbound.dll`.
  Attachment is queued onto the Qt GUI thread. The native installer deploys the
  private runtime and per-user startup hook. No external injection or service.
- `src/core/controller.cpp`: Qt GUI-thread discovery by widget object names, native
  initialization, pane registration, toggle tracking and deferred page restoration.
- `sidebarfloatsession.h`, `panefloatsession.h`: reparent existing widgets into
  floating hosts while preserving original layout slots, constraints and ownership.
  Docking restores them. Resolve owns content; Unbound owns hosts/placeholders.
- `colorclipsadapter.h`, `colorclipssidebars.h`, `colorclipstoolbar.h`: coordinate
  native Lightbox mode, main-region allocation/divider gestures and reversible
  transfer of native Color toolbar controls.
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
The shared audition-activation fixture is built and run on both backends.

Shared pane behavior and persistence live in `src/core/`. OS contracts live in
`src/platform/`; CMake selects the implementation under `src/platforms/<platform>/`.
Keep native APIs out of shared code. Windows attachment is in
`src/platforms/windows/loading/lua/helperentry.cpp`; native placement is in the Windows directory's
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
Cocoa backend loads the unchanged shared controller through
`loading/lua/`, with matching infrastructure and loader-test directories.
Keep the existing UI concept and functions.
Windows uses the Lua startup loader under
`src/platforms/windows/loading/lua/` and a native installer under `installer/`.
Its private runtime belongs in `%LOCALAPPDATA%/Unbound/Runtime`; only
`Unbound.scriptlib` belongs in the per-user Resolve `Support/Fusion/Scripts` root.
Startup must reuse one GUI-thread controller, including repeated requests.
Installer transactions require Resolve closed, always preserve settings/layouts and
must not terminate Resolve. See `infrastructure/windows/installer/README.md`.
Installer presentation is shared in `src/installer/dialog.json` and
`presentation.h`; both platform builds generate the same compiled definition.
Keep wording, control layout, display versions and presentation state there;
native installers retain platform-specific transactions and process monitoring.
Do not introduce a separate workspace/control window. Do not substitute a no-op backend for a release.
The platform contracts select one backend at compile time, with no runtime backend
selection. Core must not include platform implementation paths or OS headers.

## Build and validate

Run from this repository root:

```powershell
.\infrastructure\windows\build-release.cmd
# Override the default %USERPROFILE%\Qt\5.15.2\msvc2019_64 when needed:
.\infrastructure\windows\build-release.cmd -QtPath C:\Qt\5.15.2\msvc2019_64
```

Requires Windows x64, VS2022 C++ tools/Windows SDK, CMake, PowerShell, Python 3 and Qt 5.15.2
MSVC x64 development files. Runtime uses Resolve's Qt libraries. The script builds
the startup loader/helper and native installer, runs loader, installer and
`tests/core/pane-float` fixtures with the Windows test adapter, and audits dependencies.
Successful builds contain one self-contained `Unbound <version>.exe` installer in
`release/win/<version>/`; internal installer names also include `(yyyy.mm.dd.NNN)`.
Runtime DLLs and build manifests stay in `build/windows/`. Never overwrite published version directories.
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
Mac builds default to universal; `UNBOUND_MAC_ARCHS` selects `universal`, `arm64`
or `x86_64`, with separate build directories. Both slices use the same source,
settings and Lua hook. Audit each slice and sign only after combining binaries.
Tests explicitly select executable architecture. `UNBOUND_MAC_ALLOW_UNTESTED=1`
permits unavailable architecture execution, records NOT RUN and never suppresses
test failures. `--release` explicitly prepares a universal candidate using VERSION;
it does not tag, publish or create a ZIP. Internal runtime and packaging identifiers
use `yyyy.mm.dd.NNN` with a three-digit minimum, zero-padded daily counter on both OSes.
Top-level Mac build/test/audit/install commands delegate to Lua. Native Cocoa
fixtures require a GUI session; Windows native validation still requires Windows.
No Resolve app re-signing, public version change or automatic ZIP.
The native installer UI lives in `src/platforms/macos/installer/` and delegates to
Lua install/uninstall scripts. Package it explicitly with
`infrastructure/macos/installer/build.sh <Lua-candidate>`; its app is self-contained
and has no persistent service. Uninstall always retains settings and layouts;
installers offer no settings-deletion action. Both actions
require Resolve closed and must not terminate it automatically.

Lua loading and automatic startup work in the tested Free Resolve
configuration; pane undocking, layouts and basic Fusion interaction are validated.
See the approach's infrastructure README for installation/recovery commands and
`doc/issues/platform-validation.md` for outstanding validation.

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

## Specialized pane mechanisms

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

The geometry adapter filters retained timeline Resize events at application level,
before QLayout can resize child viewports. Resolve's custom video/audio divider
has no discovered public position setter; preserve native behavior rather than
forcing its ratio during resizing. No private layout-anchor calls are used.

Fairlight Media Pool retains its tested Qt parent/native owner relationship for
audition. Windows adds WS_EX_APPWINDOW before first show so this owned host keeps
shell identity and appears in taskbar previews/Alt+Tab. The shared audition bridge
stops before content is reparented on docking and tolerates nested notification
and target destruction. Sound Library uses the same activation correction without
this parent adjustment; its recognized player is `m_PlayerPreview`, while
Fairlight Media Pool uses `previewPlayer`.

Edit Media Pool can receive a one-pixel native slot when it is hidden before a
portrait timeline activates "Use optimized UI layouts for vertical video".
This is reproducible without Unbound. The general docking-allocation safeguard
handles the unusable slot; the warning suggests disabling that preference.
Do not automatically alter preferences, project resolution or native slot widths.

Edit Inspector excludes its native middle-eliding title label's horizontal size
hint from the floating minimum and restores its original sizing policy on docking.
Edit Index's upper tab-row background is a drag surface alongside its lower action
header; Edit Index, Tracks and Markers buttons retain normal native input.

## Settings reference

Settings are per user, across projects. Windows uses
`%LOCALAPPDATA%\Unbound\Unbound.json`; macOS uses
`~/Library/Application Support/Unbound/Unbound.json`.
`UNBOUND_SETTINGS_FILE` overrides either location. Edit JSON only with Resolve
closed, because in-memory state and autosave otherwise overwrite external edits.
Format 7 uses readable JSON and atomic writes. Unsupported/corrupt files remain
unchanged; malformed layouts are rejected rather than recalled as empty state.

`pages` is keyed by the page's Resolve object name: `WStackPage_Conform`,
`WStackPage_Color`, `WStackPage_Audio`, `WStackPage_Browse`, `WStackPage_Cut`,
`WStackPage_Fusion`, `WStackPage_Deliver`. Each page can contain:

- `mainWindow`: per-page native placement.
- `panes`: objects keyed by the catalogue's persistence name, each containing
  `floating`, optional `toolbarButtonChecked`, and `window` geometry.
- `presets`: up to five layout objects with stable UUID `id`, display `name`, and
  `configuration` containing only a `mainWindow`/`panes` snapshot.
- `activePreset`: active layout UUID; manual changes preserve it, deletion of the
  active layout or Dock all clears it. It is not part of snapshots.
- `nextPresetNumber`: next generated name suffix; deletion does not reuse numbers.

Use the pane catalogue as the authoritative key list. Edit viewers persist as
`widgetEditTimelinePlayer` and `stackedEditSourceOfflinePlayers`; Color Timeline
uses `timelineFrame`, Clips uses `m_pLightBox`. Color's shared Gallery/LUTs/Media
Pool selectors are exclusive: save at most one checked; all may be unchecked.
Clips' temporary Lightbox/Color Controls choices and sidebar widths are not
settings fields. Page focus and docking-warning acknowledgment are session-only.

Each pane `window` has these fields:

| Field | Meaning |
| --- | --- |
| `clientRect` | Qt logical desktop content x/y/width/height, excluding resize borders; authoritative for normal floating restoration |
| `normalClientRect` | Unmaximized rectangle retained beneath maximized/fullscreen state |
| `frameRect` | Recorded outer Qt frame, used to reconstruct Qt geometry in memory |
| `maximized`, `fullScreen` | Window state; minimized state is not saved |
| `screenIndex`, `screenWidth` | Qt monitor selection/scaling metadata |

Sizes must be positive; negative desktop coordinates are valid. For manual normal
geometry edits update both client rectangles. No persisted field contains Base64;
Qt's binary restoration record is reconstructed in memory. Native content minimums
and reachable captionless-window recovery apply. Moving/resizing autosaves after
a short delay; docking/shutdown flush geometry. Shutdown preserves floating intent;
manual docking or closing a floating host clears it.

Windows `mainWindow` contains `normalRect` in native placement coordinates,
`coordinateSpace: "windows-workspace"`, `maximized`, `monitors` (each with `name`,
`workArea`, `scaleFactor`) and `qtFallback` (pane-style readable geometry).
An unchanged monitor setup uses `normalRect`; changed displays use the fallback.
These coordinates differ from Qt client coordinates; keep the distinction explicit.
macOS `mainWindow` uses the pane-style readable fields with top-left logical screen
coordinates. The Cocoa adapter converts NSWindow content/frame rectangles and
zoom state; fullscreen/Spaces transitions do not replace saved normal placement.

Formats 1-6 migrate to 7 without creating layouts or changing arrangements.
Name-based layouts receive UUIDs and the active name maps to an ID; names,
snapshots and counters remain intact. Obsolete pre-format-4 `Color/m_pLightBox`
records are excluded. Layout validation requires unique IDs, not unique labels.
Direct JSON equality suppresses duplicate new layouts; no hashes are stored.
Explicit updates may overwrite an existing layout regardless of duplication.

Recall captures pending geometry/toggles before comparing snapshots, docks only
state differences before selecting incoming state, and reuses retained hosts.
Native toggle restoration runs off-first then on after float initialization.
Main placement discards its outgoing cache and uses the normal restoration path.
Source viewer intent waits for native availability. No internal divider positions
or Single/Dual Viewer selection are captured. Explicit Color recall without
floating Clips additionally returns to normal Color mode as described above.

## Platform implementation and validation

Windows startup, saved Program Viewer restoration and layouts have been validated
in Resolve Studio 21.1.1.10; Windows Free remains unvalidated. The earlier supported
Windows configuration is Resolve 21.1.0.14, Qt 5.15.2. macOS validation uses Apple M2,
macOS 27.0.1, Resolve Free 21.1.1.10 and Qt 5.15.2. Native Intel Mac operation is
unvalidated despite the universal binary. Open validation is tracked in
`doc/issues/platform-validation.md`; fixture passes are not native Resolve proof.

### Windows windowing

Captionless hosts retain native side/bottom resize margins and a one-physical-pixel
top outline. An invisible inward band adds six logical pixels for edge grabs and
twelve for corners, DPI-scaled. Native child edges route presses into the same
nonmodal resize path; hooks are removed on disposal/reparenting. No added layout
margin or overlay is used; maximized/fullscreen hosts exclude these grab zones.
`PaneResizeHitFilter` must accept only `windows_generic_MSG` with non-null message
and result pointers before any side effects. Dispatcher messages can have no
result pointer and must not start a gesture.

Windows 11 contact bounds use DWM visible-border thickness without subtracting a
pixel. Side/bottom compensation stops at content; Resolve's top client rectangle
includes the DWM outline, so top compensation uses the reported thickness even
when ClientToScreen gives no margin. Windows 10 retains one-physical-pixel
translucent-outline compensation. Main-window top snapping commits the negotiated
position after the native move loop if release clamps it by the outline thickness;
cancellation, maximization and Alt bypass do not replay the snap.
Every PaneFloatingWindow registers as a snap participant, including viewers.
Target selection uses host registration and measured contact bounds, not pane names.

Cinema Viewer from a floating Edit viewer hides the main window to match native
docked Cinema behavior and prevent duplicate application/menu transport shortcuts.
Exit restores only visibility Unbound changed; no transport-key interception or
viewer parenting changes. Docking must run the viewer-region restoration path even
after Cinema exit so content appears without requiring a main-window resize.

Native input fixtures run through `infrastructure/windows/test-input.ps1` with
30-second child deadlines and cursor-confinement release in finally cleanup.
Each Windows version-resource component is 0..65535; strings carry the complete
semantic version. UTC build IDs, version, revision, dependencies and hashes are
recorded in resource comments/manifests. Explicit `package-release.ps1` checks
clean tagged source, embedded versions and hashes and creates one enclosing folder
in `release/packages/Unbound-<version>-windows-x64.zip`, containing installer,
MIT license and source tag/commit README. It never overwrites packages.

### Cocoa windowing and toolchain

Cocoa converts bottom-left native points to shared top-left logical geometry.
Main-window gestures and floating resizing observe native frame changes, applying
shared snapping separately. Resolve owns its integrated main-title movement;
disable competing Cocoa title dragging while attached and restore it on detach.
Ordinary native title bars keep their behavior. Cocoa owns corner/resize hit tests;
floating header dragging uses pointer movement directly. Content layout is deferred
through live resize and settles on completion/cancellation. Main snap completion
runs after Cocoa and Resolve's queued Qt release, matching that gesture's timestamp
so unrelated input and placement remain unaffected.

Mac development requires Xcode command-line tools, CMake, Git and Resolve Qt,
LuaJIT and Cocoa plug-in libraries containing every requested architecture.
`UNBOUND_QT_SOURCE` and `UNBOUND_RESOLVE_APP` override toolchain paths. Pinned headers
do not replace or ship Qt runtime libraries. Rosetta may execute Intel fixtures on
Apple Silicon; that does not establish native Intel Resolve compatibility.
Live startup logs use `~/Library/Logs/Unbound/lua-bootstrap.log`; fixture output
stays under `build/macos/` and uses an isolated `UNBOUND_LOG_FILE`.

## Development constraints

- Never use hardcoded Resolve binary addresses or object offsets in production.
  Such details may be used for testing, investigation and research only; shipped
  behavior must use discoverable widgets and supported interfaces.
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
  to validate a new helper; already loaded DLLs are not updated by replacing files.
  Do not terminate the user's Resolve session without instruction.
- Tested configuration: Resolve 21.1.0.14, Windows x64, Qt 5.15.2. Other versions
  need validation. Known issue: Media Pool may widen on startup restoration and
  overlap its neighbor. Native minimum sizes apply; Nodes may need manual panning
  after docking into a narrower viewport.
- Source and symbol artwork license is MIT. Preserve applicable third-party
  notices. GammaRay is a local inspection tool and is not shipped with Unbound.

The root README.md belongs to the user. Do not add to or modify it; the user
typically edits it on GitHub and pulls those changes into the local repository.
Keep current technical documentation in AGENTS.md: describe the mechanisms,
ownership, constraints, settings and validation needed to implement Unbound.
Always communicate documentation additions or changes. This repository is
self-contained apart from the installed toolchain and Resolve.
README.md is the public GitHub landing page. Test and infrastructure READMEs may
retain commands and reproducible tool usage, with links here for technical design.
Keep only ongoing/open investigations and unsettled feature designs in `doc/`
and `doc/issues/`. On resolution, integrate the durable mechanism or constraint
here and remove the investigation document. Do not retain histories of attempts,
failed approaches, closed reports, session diaries, PIDs or build-by-build notes;
Git history retains prior work and build logs retain per-run output. Keep evidence
in an open investigation only when it helps resolve the remaining question.

## Local inspection tools

`tools/gammaray/` is ignored by Git and contains the modified JSON-export GammaRay
package, `run-gammaray.cmd`, and page snapshots. See its README for capture steps.
Edit's page widget is `WStackPage_Conform`; Color is `WStackPage_Color`, Fairlight
is `WStackPage_Audio`. Use a fresh Resolve session without Unbound for native
captures, and verify exports have `truncated: false`.
