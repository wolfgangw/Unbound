# Unbound development

## Development rules

- Keep all widget/layout manipulation on Resolve's Qt GUI thread. Preserve native
  signals, ownership, user state, page context and reversible docking. Discover
  widgets by object name; never ship hardcoded Resolve addresses or object offsets.
  Binary details are permitted only in research and diagnostic tools.
- Put pane behavior, settings, layouts, visual design and snapping policy in shared
  core. Native APIs belong behind platform contracts. Preserve feature parity and
  existing Windows behavior; never substitute a no-op backend for a release.
- Use widget/page/layout events and bounded deferred work, not recurring discovery
  scans or polling. Initialize lazy native content before automatic detachment,
  then restore saved native toggle states.
- Feature/fix work produces internal builds. Keep VERSION unchanged; do not tag,
  publish or package without an explicit request. Never overwrite published output.
- Validate a rebuilt helper in a fresh Resolve process: replacing a loaded library
  does not update it. Fixtures cannot fully model Resolve. Do not terminate the
  user's Resolve session without instruction; installation requires Resolve closed.
- Distinguish Qt logical client geometry, native placement coordinates and visible
  frame/contact bounds. Diagnose restoration errors before adding compensation.
- Do not change Resolve preferences/project resolution automatically, intercept
  transport/page shortcuts globally, or introduce an extra workspace/control window.
- Root README.md belongs to the user; do not modify it. Keep current technical
  mechanisms and development rules here, explaining each in one place. Test and
  infrastructure READMEs retain reproducible tool usage and link here for design.
- Keep only open investigations and unsettled designs in `doc/` and `doc/issues/`.
  When resolved, move the durable result here and remove the investigation. No
  histories of attempts, closed reports, PIDs or session/build diaries; Git and
  build logs retain history. Announce documentation changes.
- Source and symbol artwork are MIT licensed. Preserve applicable third-party
  notices. GammaRay is an inspection tool, not shipped code or a runtime dependency.

## Product and support

Unbound makes DaVinci Resolve panes independent, movable, resizable windows on
Windows x64 and macOS. Both platforms use automatic startup, native installers
and the same pane behavior and layouts. Universal Mac builds contain arm64 and
x86_64; native Intel Mac operation remains unvalidated and is offered for community
testing. Keep that limitation explicit in public documentation.
Windows is unsigned; macOS is ad-hoc signed without Developer ID/notarization.
These distribution limitations do not imply missing features.

| Page | Supported content |
| --- | --- |
| Edit | Media Pool, Effects, Sound Library, Keyframe Editor, Edit Index, Inspector, Metadata, Mixer, Program and Source viewers, floating Video Scopes |
| Color | Nodes, Gallery, LUTs, Media Pool, Effects, Timeline, Clips, Viewer and lower Color controls (listed below) |
| Fairlight | Media Pool, Effects, Index, Groups, Sound Library, ADR, Mixer, Metadata, Inspector; Meters excluded |

Use **layout** terminology in UI, code and tests. Preserve compatibility wire keys
and Resolve-owned names such as `btnPresetsSelector`. Tested configurations and
remaining validation are in [Platform implementation and validation](#platform-implementation-and-validation).

## Architecture

`src/core/` owns shared behavior. `src/platform/` declares contracts; CMake selects
one implementation under `src/platforms/<platform>/` at compile time. Core must
not include platform implementation paths or OS headers. Backends provide native
window rendering, events, coordinate/DPI conversion, placement and data directories.
Settings filenames, overrides, schema, migration and atomic saves stay shared.
Both platforms build the `Unbound` CMake target: `Unbound.dll` on Windows and
`Unbound.dylib` on macOS. The separate `UnboundLuaLoader` initializes that runtime.
Narrow platform compatibility workarounds live in `src/platforms/<platform>/quirks/`,
behind contracts in `src/platform/`. Keep ordinary native responsibilities in their
existing modules; an unaffected platform explicitly supplies no extra workaround.

| Owner | Responsibility |
| --- | --- |
| `src/core/controller.cpp` | Discovery, native initialization, pane registration, toggle tracking, deferred page/layout restoration |
| `panecatalogue.h` | Page/pane object names, labels, roots, native toggles, header anchors, persistence keys, migration and initialization policy |
| `sidebarfloatsession.h`, `panefloatsession.h` | Reparent native content into hosts; preserve/restore original layout slots, constraints and ownership |
| `panedecoration.h`, `paneheaderdrag.h` | Dock buttons and scoped header movement |
| `editviewers.h`, `viewergeometryadapter.h` | Edit viewer lifecycle and timeline space reclamation |
| `editscopes.h`, `nativescopescontent.h` | Edit Scopes visibility/layout lifecycle and shared exclusive ownership of the native Scopes renderer |
| `colorclipsadapter.h`, `colorclipssidebars.h`, `colorclipstoolbar.h` | Lightbox mode, Color allocation/dividers and reversible toolbar transfer |
| `panesettings.h`, `geometryjson.h`, `mainwindowplacement.h` | JSON persistence, geometry conversion, per-page placement and monitor checks |
| `pagelayoutmenu.h`, `pagefocus.h` | Layout UI and session-only page focus |
| `layoutrestorepolicy.h` | Shared page/pane exceptions for layout restoration; native state, geometry settlement and presentation integration |
| `panesnap.h`, `panewindowappearance.h`, `panegeometryrestore.h` | Shared snapping policy, host colors and reachable captionless geometry |
| Native `panefloatingwindow.h`, `panetaskbar.h`, `mainwindowsnap.h`, `windowplacement.*` | Host shell identity, OS gestures/contact bounds and native main placement |

Resolve owns transferred content; Unbound owns hosts/placeholders. Add pane metadata
once in the catalogue; specialized layout handling remains shared. New native
requirements go through platform contracts, not platform-specific pane policies.
`src/core/CMakeLists.txt` owns production sources through `unbound_link_core()`, used
by both helpers and controller fixtures. Shared fixture targets are in
`tests/core/pane-float/targets.cmake`; native operations implement `layoutadapter.h`
under each platform's tests. Audition-activation fixtures run on both backends.

## Startup and installation

Resolve's per-user Lua startup hook invokes the private loader once and returns
silently. The Qt-free loader validates the host and queues attachment on the Qt GUI
thread, reusing one controller on repeated requests. There is no external injection,
service, persistent Lua loop or visible Scripts-menu launcher. Resolve's internal
Lua exposes global `ffi` and `os.getenv`, but not `require`, `package`, `debug` or `io`.
Use Resolve's Qt runtime; do not replace it or re-sign the Resolve application.

Source lives under `src/platforms/<platform>/loading/lua/`, with matching
infrastructure/tests and `startup/` directories. Windows `helperentry.cpp` attaches
`Unbound.dll` through adjacent `UnboundLuaLoader.dll`. Runtime locations:

| Platform | Private runtime |
| --- | --- |
| Windows | `%LOCALAPPDATA%/Unbound/Runtime` |
| macOS | `~/Library/Application Support/Unbound/Runtime/` |

Only `Unbound.scriptlib` goes in the per-user Resolve `Support/Fusion/Scripts`
root. Installation migrates the old Utility launcher into backups outside the
Scripts scan tree. Install/update/uninstall require Resolve closed, never terminate
or launch it, and always preserve settings/layouts. No settings-deletion option.
Updates replace the private Runtime directory transactionally. macOS packages and
installs only the current runtime files, excluding stale `UnboundHelper.dylib`
build output; previous installations remain recoverable through rollback/backups.

Installer wording, layout, display versions and presentation state belong in
`src/installer/dialog.json` and `presentation.h`; both builds compile that definition.
Install/Update is the shared primary action. macOS uses native default-button
emphasis; Windows paints the enabled primary button with system highlight colors,
including hover and pressed feedback, without a keyboard-focus outline. Its visible
bounds, small rounded corners and thin grey border match the secondary button. Disabled and high-contrast
buttons retain native rendering; Uninstall remains secondary.
Native installers own transactions and process monitoring. See
`infrastructure/windows/installer/README.md` and the macOS Lua/installer READMEs
for build, installation and recovery commands. Isolate fixture logs with
`UNBOUND_LOG_FILE` so tests cannot pollute live startup logs.

## Pane lifecycle and interaction

The overlapping-squares button clicks to undock/dock and drags floating panes;
closing a host docks it. Strokes are muted grey (`#848484`), brightening on hover.
Tooltips are "Undock" and "Click to dock; drag to move".

`PaneUndockDrag` starts tearoff beyond Qt's drag threshold; a shorter gesture
remains a click. It owns the gesture independently of transferred/replaced buttons.
Keep initial floating size, normalize maximized/fullscreen state, and anchor the
pressed button point beneath the pointer. Keep the host transparent until anchored.
Resolve deferred targets through bounded widget events, not polling.
Raise without activation while dragging; activate on successful release only if
Resolve remains active and no modal intervenes. Preserve accessibility delivery.
The window group retains temporary priority below pins and above ordinary windows
through queued shows until release/cancellation; this is not saved pinning.
Early release uses its final position without a late drag/click. Escape, another
press, actual application departure, hiding/closing or owner destruction cancels
capture/movement, leaving detached content floating. Confirm OS departure rather
than trusting transient Qt deactivation during native Scopes reparenting.

`PaneHeaderDrag` also moves panes by plain titles/unused header background, excluding
controls and selectable/editable text. Fairlight buttons precede native options
(Create Group for Groups), without fallback strips. Edit/Fairlight Metadata decorates
all selection-state headers, including `emptyClipView/frameMetadataTop`; append the
button after the empty title in its existing row. Preserve native stack visibility.

Manual undocking disables the native visibility toggle; docking enables it, deferred
while inactive. Queue actions only for catalogued toggles/tool selectors; panes
without either complete without a pending toggle. Automatic restore honors saved
native choices after initialization. Serialize Edit Inspector/Metadata initialization
through catalogue policy so queued sidebar deselection cannot strand restoration.
Retain floating hosts across page switches; initial startup has a short fade.
Successful docking deletes empty hosts through deferred Qt deletion: never cache
empty hosts/compositors for speed. Native resize borders and shell identity remain;
Windows hosts have no title bar.

`panepagecontext.h` associates panes with logical pages, not their current dock
window. Discover `UiEditPanelAux`, `UiColorPanelAux`, `UiAudioPanelAux` at startup
and construction/show events. Auxiliary closure or Single/Dual Screen movement
must not become page departure. Edit viewer discovery retains the players container
outside main's tree and follows the persistent Edit page. No separate screen-mode
layouts, polling or forced reveals. See the Dual Screen validation note.

### Docking safeguard

After docking, a shared check observes the pane and its ancestor allocation chain.
Usable allocation completes after 100 ms without geometry/layout activity; a
collapsed allocation remains under observation for up to one second, allowing
queued native layout and main-window placement to settle. The absolute deadline
is not extended by further events. There is no polling or paint observation.
A persistently one-pixel collapsed allocation, including ancestor clipping, returns content
to its saved floating geometry without activation. Inactive pages defer this check;
respect intentional native hiding and unavailable viewers. Save actual floating
state without overwriting layout snapshots; incomplete recall can leave one dirty.
Color Timeline docking during floating-to-docked layout recall is user-validated
on Windows with this settlement mechanism.

The affected button pulses between muted grey and playhead red until its own menu
opens, then stays muted. Tooltip/menu guidance remains until successful docking;
another failure restarts the pulse. Animate only while unacknowledged, visible and
the app is active. No modal warning or forced native slot widths.
Edit Media Pool can collapse when hidden before a portrait timeline activates
"Use optimized UI layouts for vertical video", reproducible without Unbound.
Its guidance suggests disabling that preference; other panels use general guidance.

### Activation and page focus

Activating a group member raises visible, non-minimized peers without changing
focus. Automatic reveals set `WA_ShowWithoutActivating` before native creation;
manual undocking activates explicitly. Dialogs, popups, other applications and
newer focus choices take priority.

`panewindowgroup.h` orders `keepAtop` hosts above ordinary Resolve windows while
a member or its layout menu is active. Membership includes the current top-level
owners of catalogued auxiliary pages, including unnamed secondary wrappers.
Pins use session-only activation MRU order; restored hosts join in catalogue-name
order. Ordinary activation preserves pin order. Menus retain popup priority and
must not lift main above their invoking pane or pins. Reordering never activates
windows or changes ownership/registration. No idle polling.

Windows keeps ordinary OS window levels and checks actual foreground ownership.
Suppress preliminary ordinary/menu raises and nonactivating pin raises with
`WM_WINDOWPOSCHANGING`/`SWP_NOZORDER`, then submit final pinned/selected/peer order
in one `DeferWindowPos` batch. Also guard nonactivating main raises while an Unbound
menu owns foreground or a floater does, even without pins. Preserve genuine native
activation and size/ownership flags. Never demote pins temporarily or insert ordinary
hosts after the menu's topmost handle. Cocoa pin levels are described below.

Main minimization conceals only currently visible, non-minimized Unbound hosts;
restore that set only on available pages, preserving Qt visibility intent, window
state and focus. Windows uses nonactivating visibility flags; Cocoa uses
orderOut/orderFront. Concealed hosts leave the taskbar. Native auxiliary minimization
remains Resolve-owned; independently hidden/minimized panes remain so.

New panes open centered in the work area containing their native center, falling
back to the window's screen. Remembered positions take priority. After show/restore,
check actual client bounds including minimum-size growth: retain visible spans across
adjacent displays, otherwise fit to the work area with greatest overlap. Oversized
native minimums remain intact with top-left controls reachable. Never correct during
manual move/resize.

`pagefocus.h` remembers the last focused window/control per page for the session.
After queued reveals, restore an available target or retain native main focus.
Ignore navigation/hide-show churn; newer input, app departure, dialogs or a newer
page change cancel deferred recall. Focus is not settings/layout data.

### Floating geometry restoration

`PaneGeometryTransaction` retains requested geometry through startup, layout recall
and page return. Native layout can temporarily grow minimums without later shrinking
the host. Reapply intended geometry when current constraints permit; never override
those constraints or add fixed offsets. Page return uses outgoing working geometry,
including unsaved changes, not the saved layout.

Observe layout requests until one second of inactivity, capped at three seconds.
New requests supersede old ones; input, geometry gestures, hiding, state changes and
docking cancel. Only queued corrections contribute to transition readiness.
Persistent minimums and monitor recovery can legitimately leave a layout dirty.

`PaneMainResizeGeometry` starts the same transaction on main Resize events, preserving
pre-growth floating geometry across responsive native layout changes and page return.
Explicit recall replaces the intention; host input, native gestures and window-state
changes discard it. Both standard hosts and Edit viewers use this shared, session-only
mechanism, with no work/polling between main resizes.

### Snapping

Main/floating windows snap to edges, including distant alignment. Per axis choose
smallest adjustment, ties by nearer window; deduplicate edges. Work-area edges
require perpendicular overlap; window edges win equal adjustments. Zones are 24
logical pixels; Alt bypasses. Animate only capture/release corrections with 32 ms
quadratic ease-in at 16 ms cadence; direct movement remains immediate. Release
commits exact geometry; cancellation/Alt clears animation. Respect native constraints
and measured contact bounds.

Shift uses `panegridsnap.h`: 24-logical-pixel grid at the pointer work area's origin
(nearest display when outside). Moves align left/top; resizes affect manipulated
edges within constraints. Invalid grid targets retain native geometry. Use two-pixel
midpoint hysteresis and the same easing; reset state at gesture/mode boundaries.
Alt overrides both modes.

`panegridguide.h` shows a nonactivating, input-transparent grid plus other pages'
working main/floating placements, including unvisited pages. Exclude docked panes,
current page and archived layouts; outlines are references, never snap targets.
Read in-memory settings once, recover missing-display placement through shared
geometry adapters, and deduplicate rectangles. Red outlines have 10 px corners,
Gaussian alpha sigma 2 over a six-pixel reach; combine alpha by maximum, not addition.
Cache the DPI-scaled raster until references/displays change; release it after a
100 ms reversible fade-out. Retain only the hidden overlay for reuse; destroy with
its gesture owner. Animate only during fades. Windows uses DPI-scaled units and a
gesture-scoped GUI-thread modifier hook that never consumes keys; Cocoa uses logical
points and flags-changed events. No idle polling.

## Specialized pane mechanisms

### Edit viewers

Program and Source use normal header buttons and independent geometry/floating
intent. Source restoration waits for native availability; do not force Single/Dual
Viewer Mode or add a separate control window. When no visible viewer stays docked,
`viewergeometryadapter.h` expands the timeline using retained native baselines.

Both floating Edit viewers use the activation bridge described under Audition players;
native focus continues to select Source versus Program/timeline transport.
Bind before first host show and stop before docking. Program activation also needs
the page command context restored when Source was the previously selected player.

During resizing, an application filter scoped to reclamation intercepts retained
timeline Resize events before QLayout processes child viewports. Coalesce interim
half-height allocations, preserving the last delivered oldSize. Finish the native
resize pass once through the retained ancestor chain up to main, then synchronously
apply expanded regions. Defer main-region painting for this transaction; floating
viewers keep updates. Pane docking and native toggles can also assign regions
without resizing an ancestor. While reclamation is active, the first retained
region Move/Resize holds root painting until the queued correction finishes.
Also defer main-window `UpdateRequest` delivery during the transaction: disabling
child paints alone still permits backing-store presentation of cached pixels at
interim geometry. Repost one deferred request after restoring the final geometry;
coalesce the geometry burst and release on completion, page hide or disabling
reclamation. Preserve an already-disabled update state. This is scoped to Edit
viewer reclamation, not particular pane buttons or global input. Docking restores
native geometry. Do not call private layout
anchors or force the native video/audio divider ratio; no public setter is known.

In Single Viewer Mode, Media Pool hover hides Program and shows Source. Keep a
floating Program independently visible through a coalesced GUI-thread update after
that swap. Preserve native visibility intent for docking, ignoring our own shows
and ownership changes; Source hiding marks return to Program even when Resolve's
show is already satisfied. Page/host hiding still wins. No frame copying, playback
interception or changes to native Source preview/viewer mode.

### Edit Scopes

`editscopes.h` adopts native `UiWaveformWindow` directly when opened on Edit through
Workspace > Video Scopes. Observe the catalogued `workspaceVideoScopesToggle`
action and named native window events; preserve native command/shortcut routing
and renderer initialization. No extra adoption click, toolbar toggle or dock slot
is introduced. Add the Unbound button before the native window options, with
header/button dragging, snapping, Always on top and the Edit layout menu.
Explicit Workspace On activates the adopted host like manual undocking, subject
to application/modal priority and normal pinned-window ordering. Startup, layout
recall and page return remain nonactivating; remembered geometry does not change
this distinction.
The button click, host close, Workspace Off and Dock all close Scopes; no native
docked allocation check applies to this floating-only lifecycle.

Persist Edit visibility and geometry under its own `UiWaveformWindow` pane entry;
older layouts omit it and therefore recall with Edit Scopes closed. Existing
settings/snapshots require no migration or rewriting. Native opening dirties the
active Edit layout normally. Startup and layout recall initialize Scopes through
its native action. Page departure retains Edit's floating intent and host, closes
the native renderer while visible, and returns native ownership before hiding the
host. Color can then acquire that same renderer through `NativeScopesContent`.
Page release runs in the page's object filter, after application-level transition
capture. The application-wide native-window observer only schedules adoption;
it must not release content or hide hosts before that capture.
Edit return reacquires it with Edit's placement; neither page writes the other's
visibility, geometry or layout. Closing releases the empty host. The shared
geometry restoration and main-resize observers apply to the Edit host too.
Fixtures cover QWidget/QDialog shells, native commands, closing, page handoff,
geometry, startup and layouts. Windows explicit opening/activation and outgoing
transition coverage are user-validated. Broader Windows lifecycle coverage, macOS
and coexistence with native Dual Screen allocations remain pending. This manages the global
floating window, not a separate secondary-screen docked Scopes view.

### Pane command activation and audition players

`paneactivationbridge.h` restores Resolve's command context on genuine floating-host
activation. Catalogue scope: Media Pool on Edit/Color/Fairlight, Edit Keyframe Editor,
Color Nodes/Gallery/Timeline/Keyframes, Edit/Fairlight Sound Library and both Edit
viewers. Bind before first show to the logical page's main window and visible content;
standard panes use host visibility because their native slot is hidden.

Send one main-window `WindowActivate` through application notify, then consume that
exact event before QWidget propagation. Native focus chooses the subcontrol/player;
no key forwarding, forced focus, click or child-focus notifications. Retained hosts
keep their bridge, gated by actual host activation and page/content visibility.
Exclude hidden/minimized hosts, unrelated windows, popups and modal dialogs. Stop
before docking; tolerate nested notification/destruction. Expanded palettes and
Scopes need no bridge. Local text editing alone does not justify one.

`auditionplayeractivation.h` handles Sound Library's separate audio-player selection
on Edit/Fairlight. After host activation settles, one queued callback briefly focuses
`m_PlayerPreview`, then restores the selected control, matching docked native behavior.
Skip a focused preview/descendant. Preserve field/list selection and playback position;
no native player methods or transport keys. Hide/deactivation cancels queued work;
page visibility, ownership, modal/popup and active-host guards apply. Reentrant
docking/destruction or newer focus prevents restoration. Delete before docking.
Header focus does not imply list focus: native Up/Down can target the timeline.

Fairlight Media Pool's `previewPlayer` retains its Qt parent/native owner. Windows
adds `WS_EX_APPWINDOW` before first show for taskbar/Alt+Tab identity. Sound Library
requires no preview-parent change. Outstanding validation is in
`doc/issues/floating-pane-keyboard-routing.md`.

### Color Timeline

Transfer `UiMiniTimelineWidget` independently of Clips. Docked Timeline keeps its
top-left corner button. Floating Timeline gains a 28-logical-pixel shared header
with its title and the same Unbound button; title/background use `PaneHeaderDrag`.
`colortimelineheader.h` owns only the extra host row and returns the button before
docking. Native ruler/track-gutter input remains playhead interaction, with no
added native docked constraints or header. The observed 76 px
slot height is configuration-dependent, not a fixed maximum. Floating minimum
width is 240 logical pixels for labels/clip bars; native height resizing, track
scrolling and clip selection remain Resolve's. Native resizing/selection are
validated on Windows.

### Color tool selection

`ColorToolSelection` retains the checked, catalogued selectors in `frameColorBottom`
on page departure, before native Scopes handoff changes them. Restore through native
button actions after queued page-return work, preserving every visible tool region
without assuming a two- or three-region arrangement. Observe selector changes for
at most one second after return; coalesce corrections without polling. New input,
explicit layout recall/Dock all, app departure and teardown cancel restoration.
Rapid page switches retain the outgoing intent rather than intermediate native
choices. Hidden/disabled controls remain native. Only queued correction work enters
transition readiness. Selection memory is session-only, with no settings/schema or
layout-snapshot changes; main-window hiding/minimization is not page departure.
Tracker/Info preservation across Edit/Color page return is user-validated on macOS.

### Color Clips

Resolve shares `m_pLightBox` between Color and Fusion. On Color departure return
content to its retained native layout, hide the placeholder and retain Color's host,
geometry and intent. Transfer back before revealing on return. Graph recovery and
inactive docking must respect temporary native ownership. Remove Color decoration
outside Color context, even when docked; never duplicate native content.

Floating Clips uses native Lightbox rendering/filtering/thumbnail sizing, ruler and
scrolling. Minimum size is 360 x 240 logical pixels; Windows reflows on resize release.
Its button occupies the top-left 32 px gutter with 4 px top inset. Gutter and unused
header drag without covering native controls; docked placement keeps native alignment.

Before entering Lightbox, initialize native Timeline in normal Color mode and capture
constraints after queued layout. Hidden collapsed geometry is not a baseline.
`NativeToggleState` preserves actual toggle intent across this temporary initialization
for settings/dirty detection while native signals still run. Restore/release on finish
or cancellation; hidden pages defer, docking cancels.

The adapter fills unused Lightbox allocation with Viewer/optional Timeline, reserving
left Gallery/LUTs/Media Pool and right Nodes/Effects according to native toggles and
constraints. Transfer Color controls into a native row above the retained toolbar;
restore ownership/order on docking and suppress interim repaint.
Initialize all incoming Color panes, including lazy discovery, before Lightbox entry.
Suspend retained Clips until those attempts finish, even for incoming docked layouts.
Manual docking restores pre-float Lightbox/Color Controls choices; native Lightbox exit
docks Clips. Explicit recall without floating Clips and Dock all request normal Color
mode. Dock all shares recall's suspension/playback lease through queued discovery.
These mode choices and sidebar widths add no settings fields.

`colorplaybackpause.h` supplies optional bounded leases for required Lightbox mode
changes. Nested leases share one stop; idle playback and clean floating-Clips recall
need none. Discover native transport by object name in docked/floating Viewer.
Both direction buttons unchecked means idle, regardless of Stop. Resume intent
requires Stop acknowledgement with both directions cleared. Unknown/unavailable or
unacknowledged transport simply skips assistance: never refuse layout/docking work.
Final release queues two continuations before resuming only playback stopped by the
lease. New input/transport, page/project/app departure, dialogs, failure, shutdown or
three-second expiry cancels resume. Preserve position and looping; no polling.

Read speed from public `labelPlaySpeed`; explicitly hidden text means 1x. Resume the
original direction, then a bounded count of power-of-two steps through native
`controlPlayForward`/`controlPlayReverse` actions. Do not use asynchronously updating
label text as loop feedback. Unknown speeds/actions fall back to 1x, same direction.
These are public Qt operations, not a Resolve scripting transport API.

#### Color sidebar divider gestures

Native Lightbox mode does not provide normal sidebar sizing. For a recognized
left/Nodes/Effects divider, freeze main top-level backing-store and Clips-host
repaint, temporarily enter normal Color mode, then deliver ordered mouse events
after its queued layout pass. Map global coordinates to the current handle,
capture native widths, restore Lightbox/allocation and repaint on release.
A separate nonactivating, input-transparent two-logical-pixel grey guide follows
the native divider, including its clamps. Dispose of it on finish/interruption.
Generation guards discard stale queued input; release only native gestures already
started. Restore prior repaint state on docking, page hide, grab loss, deactivation
or target destruction. Widths remain native/session state, not layout snapshots.

`windowpresentation.h` supplements the freeze for native composited surfaces.
Windows uses the Qt backing store. Cocoa prepares one composited image of each
affected window before mode/input changes because Qt update suppression cannot
freeze Metal surfaces. Preserve input order during asynchronous preparation.
ScreenCaptureKit discovers only current-process windows without TCC consent
(macOS 14.4+; tested Resolve requires macOS 15). In-memory covers pass input through;
remove after restored layout/repaint delivery, or immediately if cancelled before
preparation completes. Failure leaves mode/widths unchanged and restores updates.
No persistent capture stream, screen-access request, polling or private offsets.

### Color viewer and lower controls

Transfer Color `UiMainViewerForm` from `m_pFramePlayer`, retaining outer
`m_pFramePlayerContainer` for native layout/Clips. Native header/status/tool-page
backgrounds use `PaneHeaderDrag`. Viewer and lower-region reclamation are deferred.

Supported tools: Camera Raw, Color Match, Color Wheels, HDR, RGB Mixer, Motion Effects,
Curves, ColorSlice, Color Warper, Qualifier, Window, Tracker, Magic Mask, Blur, Key,
Sizing, 3D, Keyframes, Scopes and Info. Catalogue scope disambiguates repeated names
and identifies stack family, header and selector. Preserve stack entries/indices.
Camera Raw/HDR transfer the multi-entry layout; ordinary tools transfer single
content while preserving native layout. Key, Sizing and Tracker retain `keyWidget`,
`sizingWidget`, `trackerWidget` as the first widget in their native tab layouts and
transfer those containers' contents instead. Native tool lookup requires their
identity in the original slot; put placeholders inside them.

Curves/Warper use `ColorNativePalette`: invoke native Expand, then embed the registered
`UiFloatingPaletteWidgetDialog` inside the host. Native registration keeps signals
updating independently of the selected tool. Clear only its explicit window minimum;
retain descendant constraints/hints and restore the minimum before native return.
Already-expanded palettes follow the same path. Docking restores parent/flags and
calls public `QDialog::reject()` (hidden-dialog close need not return native content).
Defer native Close/Escape into session docking; rejected returns retain a usable host.
Keep native toolbar expanded state and replacement-tool selection. Histogram Input
versus Output and Warper mode remain user/native state; Key needs no expansion.

`ActiveStackSize` ignores inactive `m_WidgetPanel` mode hints while floating:
Warper relaxes both axes, ordinary Color tools only height. Preserve descendant
constraints and ordinary tools' horizontal budget. On mode/child/layout changes,
coalesce policy/minimum refresh through the host; restore active-page policies and
all policies before docking without overwriting newer native changes. Larger modes
may grow the host; manual shrinking remains available when constraints permit.
Color Wheels separately ignores allocated wheel-stack width hints, deriving its
minimum from intrinsic group hints plus the tightest mode's spacing budget. Expanded
bars gaps/allocated minimums must not inflate that minimum. Preserve padding/child
constraints and restore stack policy/minimum on docking.

`ColorNativeScopes` uses `NativeScopesContent` to embed Resolve's independent global
`UiWaveformWindow`, opened through `btnUndock` or adopted if already open. Discover
as QWidget (Windows QDialog, macOS QWidget). Leave the single docked scope in its
slot without a placeholder. Retain native renderer registration, single/dual/quad
controls and settings; clear only explicit window minimum. Docking invokes `btnHide`
while visible, then restores parent/flags/geometry/minimum. Close/Escape/Hide defer
session docking; page hiding retains floating intent.

On Color departure, return the global window to native ownership before hiding its
host. Restore the previously borrowed native visibility/geometry and remove Unbound
chrome/guards. A native close must happen while content is visible, otherwise Resolve
can reopen Scopes on the next page. Visible native return reapplies saved top-level
geometry after Show handling. Color return captures updated native state before
embedding again. Inactive-host retirement must not change another owner's intent;
main hiding/minimization is not page departure. Edit adoption uses its separate
lifecycle above. No forced main Scopes selection or renderer copying.

`widgetquirks.h` refreshes macOS embedded Scopes `titleFrame`/`headerFrame` after
queued show/content-move layout: interim top-level positioning can clip header paints.
Hide/reparent/destruction cancels. Do not change native geometry/focus/renderer state;
Windows needs no extra refresh.

`ColorScopesLayoutRefresh` updates native view-count availability after coalesced
resize/show through a zero-distance press/move/release delivered directly to public
`QObject::event`. Restore exact allocation and last non-resize cursor before returning;
never retain a cursor sampled during a border gesture. Direct delivery bypasses
application menu/focus/geometry filters. Defer until real mouse release, cancel on
hide and detach before docking. While embedded, consume gestures on the native
window itself to prevent independent inner resizing; descendants keep native input,
including scope wheel panning into empty plot space. The direct refresh bypasses
this guard. No private slots, offsets or idle timer.

Place buttons in existing headers before options; nested selector variants mirror
native visibility and use `PaneHeaderDrag` on passive child frames. This includes
Sizing `cbWidgets`, Tracker `cbBoxTrackStableSelection`, Blur/3D `cmbSetTo`, Wheels
`cbColorWidgets`. Tool selectors are not visibility toggles: never switch them off
for reclamation; docking selects the tool, deferred on inactive pages.
`ColorExpandGuard` disables recognized Expand actions only while floating, including
retained docked Scopes controls. Observe enabled/show and bounded child discovery;
restore original enabled states before native return. Docked expansion stays native.
Outstanding mode/material/platform checks: `doc/issues/color-controls.md`.

### Other pane constraints

`PaneElidingLabels` relaxes only the horizontal full-text hint of native
middle-eliding labels identified in the shared pane catalogue. Edit/Fairlight
Inspector titles and Edit Metadata's clip/photo filenames use it, including
repeated single/multi-selection variants. Resolve retains text and painting;
vertical policies, stretch and other content minimums remain intact. Scoped child
and reparent events coalesce discovery of late variants. Docking/teardown restores
original policies without overwriting a newer native policy. Media Pool keeps its
native sizing behavior. Edit Inspector/Metadata filename elision and serialized
cold-start restoration are user-validated on Windows and macOS.

Edit/Fairlight Index tab-row backgrounds also drag; tab/action controls retain
native input.

## Layout menu

Right-click an Unbound button for up to five per-page layouts. Save creates a
snapshot unless identical; only explicit update overwrites. UUID identity survives
renaming and dirty edits; names may repeat. Active layouts stay highlighted; dirty
ones show a save icon left of trash. Recall discards unsaved outgoing edits; clean
recall is a no-op after capture except the Color mode rule. No draft copies.

Dock all affects only the current page: dock hosts, cancel pending float intent and
clear active identity, preserving snapshots/main placement/other pages. Its row always
exists and is disabled without an actual host (pending intent alone does not qualify).
The final separated row is `Always on top`, disabled while docked but retained for
next undock. It persists without a layout and participates in snapshots/dirtiness.
Missing means false; remove false fields so old snapshots become clean. Loading old
files never rewrites them; first use changes only the working pane.

The main-owned Qt tool QFrame sits outside Qt's transient popup stack. Anchor below
the invoking button, clamp to work area, retain UUID-keyed rows and coalesce updates.
Save/recall/update/delete stay open. Outside interaction, Escape, actual OS application
departure, close or owner hide/minimize dismiss; hover and transient Qt deactivation
do not. Explicit visibility intent prevents native tool-window sweeps from flashing
or reopening the menu. Observe native departure only for menu lifetime (Windows
foreground events; Cocoa app resignation/menu tracking). Growing status rows reclamp
without reopening. `WindowInteractionEvent` precedes user move/resize, including
native-child edges; programmatic recall geometry does not emit it.

Use main background palette, 4 px popup/3 px highlight corners, 5 px control gaps,
2 px vertical margins and 30-logical-pixel row controls without theme vertical padding.
Dirty save is playhead red; trash neutral; active highlight survives hover/focus.

A one-second hold edits a name; release/leave/hide/drag cancels the timer and completed
hold suppresses recall. Activate/focus/select editor text. Enter renames only;
Escape/focus loss cancels. Trim names, reject blanks, limit to 128 characters.
While editing, deliver only this popup's keys/shortcuts directly to its Qt handler
rather than re-entering Resolve via sendEvent. Plain Left/Right collapse selection;
modified keys remain native. No synthetic clicks/global shortcut changes; clear
callbacks on teardown.

### Layout transition presentation

`layoutrestorepolicy.h` centralizes diagnosed pane/page exceptions: native Color mode
and playback assistance, expected palettes, geometry settlement and temporary toggle
intent. It contains no platform APIs. Persistent native minimums remain authoritative;
never hide dirtiness or rewrite saved snapshots to mask a restoration difference.

`LayoutTransition` owns begin, readiness/invalidation, reveal and cancellation.
`layouttransitionpresentation.h` supplies optional platform capture/presentation,
buffer accounting and actual app-departure notification. Failure/unavailability leaves
normal layout work running without a visual wait. Release resources on every exit;
never request capture permission. macOS uses `NoLayoutTransitionPresentation` and
retains native presentation. Alternative mechanisms use the same contract.

Windows captures all displays with public GDI into nonactivating covers, in release
and internal builds. Images stay in memory; raw bitmap storage is capped at 128 MiB
(compositor memory is additional) and freed on completion/cancellation. External video
briefly appears frozen while playback continues. Reveal fades over 120 ms.
Outside input, real dialogs, application departure and superseding transitions cancel;
a three-second GUI-thread deadline cannot preempt a blocked GUI thread. Nonmodal
`UiWaveformWindow` handoff is an expected exception, including before the destination
page is known. Modal Scopes still cancels.

The existing layout menu remains interactive above covers. Temporarily exclude supplied
popup windows using `SetWindowDisplayAffinity`, restore prior affinity on every path,
and insert covers below them without activation. Never hide/reopen or copy the menu.
Hiding/destroying a supplied popup cancels. Capture exclusion requires Windows 10
2004+; failure skips protection, not layout work. See the Windows 10 caveat in
`doc/issues/layout-transition-readiness.md`.

`LayoutTransitionRecall` uses `layoutreadiness.h`: tracked queued work, 50 ms structural
quiet and up to three final-update attempts. Public GL composition must match current
geometry revision; raster acknowledgement follows UpdateRequest delivery. Windows
flushes DWM before fading. This is bounded evidence, not a native renderer-completion
guarantee. Idle observers/paint activity alone do not delay reveal. Unavailable Source
intent does not hold recall open. Retain pending work across observer generations;
release filters/connections on completion, cancellation or unavailable capture.
macOS enables neither observer nor cover. No production transition diagnostics.

`pagetransition.h` observes the named page stack. Cover if either page has floating
content/intent: capture outgoing Hide before pane filters remove hosts, otherwise
incoming Show before restoration. Carry departure capture forward without recapture;
coalesce duplicate Hide. Fully docked-to-docked switches allocate no cover/observer.
Destination readiness includes its queued reveals, geometry and main placement,
not outgoing hosts/other pages. Page departure cancels; page transitions acquire no
Color playback lease or native layout mutations. Shared cancellation rules apply.

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
  `floating`, optional `toolbarButtonChecked` and `keepAtop`, and `window` geometry.
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
`MainWindowPlacement` retains the native normal placement while maximized and
reapplies the requested normal placement once, deferred until the state change
has established the normal frame. Windows can initially enforce a client minimum
using the outgoing maximized frame margins, incorrectly growing an otherwise valid
normal rectangle. The shared settlement also follows saved normal-page restoration
and delegates frame handling to the platform adapter. New page/recall, special
window states, input or a geometry gesture cancels pending correction. Preserve
native minimums and the exact saved rectangle; never subtract a fixed offset or
ignore a small discrepancy in dirty detection.
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
state differences before selecting incoming state, and retains hosts for panes
that stay floating in both layouts.
Native toggle restoration runs off-first then on after float initialization.
Main placement discards its outgoing cache and uses the normal restoration path.
Source viewer intent waits for native availability. No internal divider positions
or Single/Dual Viewer selection are captured. Explicit Color recall without
floating Clips additionally returns to normal Color mode as described above.

## Build and validate

### Shared output convention

The development/release process is the same on both platforms:

- Internal development builds keep VERSION unchanged and use `yyyy.mm.dd.NNN`,
  with a daily counter zero-padded to three digits. Each build has its own folder
  directly under `release/win/` or `release/mac/`. The installer is named
  `Unbound <version> (yyyy.mm.dd.NNN)` with the platform's `.exe` or `.app` suffix.
- Public builds require an explicit release request and go in `release/packages/`.
  Distribution filenames include the semantic version, platform and architecture:
  `Unbound-<version>-windows-x64` or `Unbound-<version>-macos-universal`, with the
  appropriate installer/archive suffix. Public names have no internal build number.
- Never overwrite an existing build/package. Verify versions and payload hashes;
  preserve the signed macOS bundle. ZIP creation, tagging and publishing are
  separate, explicitly requested actions. Build output is Git-ignored.

These are final deliverable locations. A script's intermediate runtime candidate
or installer staging directory does not replace this convention; finish by placing
the installer/package at the prescribed destination. Keep build entry points under
`infrastructure/<platform>/`, not the repository root.

### Windows

For normal feature/fix development:

```powershell
.\infrastructure\windows\build-release.cmd -InternalBuild
# Optional Qt override:
.\infrastructure\windows\build-release.cmd -InternalBuild -QtPath C:\Qt\5.15.2\msvc2019_64
```

Requires Windows x64, VS2022 C++ tools/Windows SDK, CMake, PowerShell, Python 3,
and Qt 5.15.2 MSVC x64 development files (default
`%USERPROFILE%\Qt\5.15.2\msvc2019_64`). The script builds loader/helper/installer,
runs loader, installer, shared pane/layout and native Windows fixtures, then audits
dependencies. Native input fixtures use `test-input.ps1` with 30-second child
limits and cursor-confinement release in finally cleanup.

Internal output: `release/win/yyyy.mm.dd.NNN/Unbound <version> (yyyy.mm.dd.NNN).exe`.
Public output: `release/packages/Unbound-<version>-windows-x64.exe` (EXE alone).
Embedded semantic metadata and the fourth Windows numeric revision identify
internal builds. Runtime DLLs/manifests stay in `build/windows/`.
Icons regenerate from `src/assets/unbound-symbol.svg`; shared artwork stays there,
Windows ICO/resources under `src/platforms/windows/resources`.

For an explicitly requested release, omit `-InternalBuild`. VERSION supplies the
semantic version; `-Version` overrides it, `-BuildNumber` changes only numeric
revision (default 0). Each numeric component must fit 0..65535. Full semantic
strings, UTC build ID, revision, dependencies and hashes remain in resources/
manifests. The current script stages the public installer in
`release/win/<version>/Unbound <version>.exe`; copy it to the public output above
and verify embedded version and manifest/copy hashes.

ZIPs are optional and require explicit request. `package-release.ps1` checks clean
tagged source, embedded versions and hashes, then creates
`release/packages/Unbound-<version>-windows-x64.zip` with one enclosing folder
containing installer, MIT license and source tag/commit README.

### macOS

Requires Xcode command-line tools, CMake, Git and Resolve Qt/LuaJIT/Cocoa libraries
containing requested architectures. `prepare-qt.sh` prepares pinned Qt 5.15.2
headers; no replacement Qt runtime is shipped. `UNBOUND_QT_SOURCE` and
`UNBOUND_RESOLVE_APP` override paths. Commands live under `infrastructure/macos/`;
top-level build/test/audit/install scripts delegate to Lua.

Internal output: `release/mac/yyyy.mm.dd.NNN/Unbound <version> (yyyy.mm.dd.NNN).app`.
Public output: `release/packages/`, using the distribution basename
`Unbound-<version>-macos-universal` and the appropriate `.app` or requested archive
suffix. The public installer inside an archive is `Unbound <version>.app`.

`build.sh` builds, tests, audits and signs runtime candidates. Universal is default; `UNBOUND_MAC_ARCHS` selects
`universal`, `arm64` or `x86_64`, with separate build directories. Audit each slice
and sign after combining. Tests select executable architecture explicitly; Rosetta
execution is not native Intel Resolve validation. `UNBOUND_MAC_ALLOW_UNTESTED=1`
permits unavailable execution, records NOT RUN and never suppresses actual failures.
Cocoa fixtures require a GUI session; Windows native validation requires Windows.

Explicit `build.sh --release` prepares a universal candidate from VERSION without
tagging, publishing or ZIP creation. Build the self-contained native installer with
`infrastructure/macos/installer/build.sh <Lua-candidate>`; it delegates transactions
to Lua scripts. Current scripts stage runtime candidates under `release/mac/lua/`
and installers under `release/mac/installer/`; place the completed internal app
or public deliverable at the final output above. Use the runtime's internal build
identifier for both its final folder and app name, not a separate packaging-run
counter. Installer creation does not automatically create a ZIP.
When explicitly requested, create the public ZIP with `ditto --keepParent`,
containing `Unbound <version>.app` at its root. Verify the extracted bundle's
signature, payload and both architecture slices before distribution.

## Platform implementation and validation

Windows startup, saved Program Viewer restoration and layouts have been validated
in Resolve Studio 21.1.1.10 and on Windows with Resolve 21.1.0.14; Windows Free
remains unvalidated. macOS validation uses Apple M2, macOS 27.0.1 and Resolve Free
21.1.1.10. Both platforms use Qt 5.15.2. Native Intel Mac operation is
unvalidated despite the universal binary. Open validation is tracked in
`doc/issues/platform-validation.md`; fixture passes are not native Resolve proof.
Color cold-start restoration, including floating content, Fairlight cold startup
with floating Media Pool/Sound Library, and Fairlight audition are user-validated
on macOS. General Color simultaneous updates, resizing, docking and layout recall
are also validated; broader mode/material combinations remain open.

Native minimum sizes apply. Known limitations include Media Pool widening and
overlapping a neighbor during startup restoration, and Nodes requiring manual
panning after docking into a narrower viewport. `nodescanvasbounds.h` maintains
scene bounds as the Nodes viewport changes.

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
viewer parenting changes. If Resolve has already returned activation to the floating
viewer, preserve that host and focused control across main-window restoration.
A subsequent main activation queues one correction, bounded to one second and
cancelled by newer input, another window activation or application departure.
Require main to remain foreground and no modal dialog or popup before correcting;
the normal host activation bridge then restores Resolve's command context.
Docking must run the viewer-region restoration path even
after Cinema exit so content appears without requiring a main-window resize.

### Cocoa windowing

Cocoa pins use `NSFloatingWindowLevel` while the active native key window belongs
to the Resolve group. The shared policy still selects pins and their MRU order.
A visible Unbound layout menu receives the next level above those pins. Native
application/key-window notifications and scoped Qt visibility/property/state events
restore original levels on app departure, unrelated/dialog focus, hiding, unpinning,
minimization, native window recreation and group teardown. Levels never change
focus, ownership or geometry. This prevents an ordinary main-window click from
covering pins before a queued ordering correction; no click interception,
method replacement, screen capture or polling is used.

The additional Color toolbar row reserves space for native traffic-light buttons
when they overlap its allocation. The Cocoa adapter measures visible standard
window-button bounds in logical screen coordinates and adjusts only the row’s
left margin, retaining its original padding. Scoped layout/geometry/window-state
events refresh clearance; non-overlapping rows and Windows keep original spacing.
Docking removes the adapter and restores the original margins. No saved geometry
or native title-bar appearance changes.

While the layout popout is visible, Cocoa observes native application-menu tracking
and invokes shared popout dismissal immediately. Remove the observer on hide or
destruction; native menu updates and unrelated editing context menus do not dismiss
it. Windows retains the shared Qt outside-input path.

Cocoa converts bottom-left native points to shared top-left logical geometry.
Main-window gestures and floating resizing observe native frame changes, applying
shared snapping separately. Resolve owns its integrated main-title movement;
disable competing Cocoa title dragging while attached and restore it on detach.
Ordinary native title bars keep their behavior. Cocoa owns corner/resize hit tests;
floating header dragging uses pointer movement directly. Shift is reserved for the
shared grid: copy native resize mouse/modifier events with Shift removed before
Cocoa handles them, preventing its proportional resize. Preserve physical Shift
state for grid selection and leave other modifiers, content clicks and keyboard
shortcuts unchanged. Shift-grid resizing is user-validated on macOS.
Content layout is deferred
through live resize and settles on completion/cancellation. Main snap completion
runs after Cocoa and Resolve's queued Qt release, matching that gesture's timestamp
so unrelated input and placement remain unaffected.

Main placement captures/applies native NSWindow rectangles, including Resolve's
integrated title area. Do not use QWidget::restoreGeometry for main: its assumed
external title-bar clearance breaks screen-edge placement. Keep shared geometry
fields and monitor recovery; Cocoa handles frame/content conversion and zoom.

Live startup logs use `~/Library/Logs/Unbound/lua-bootstrap.log`; fixture output
stays under `build/macos/` with an isolated `UNBOUND_LOG_FILE`.

## Local inspection tools

`tools/gammaray/` is Git-ignored and holds the modified JSON-export GammaRay package,
`run-gammaray.cmd` and page snapshots; see its README for capture steps. Use a fresh
Resolve session without Unbound for native captures and verify `truncated: false`.
Edit root is `WStackPage_Conform`, Color `WStackPage_Color`, Fairlight
`WStackPage_Audio`, Fusion `WStackPage_Fusion`. The repository is otherwise self-contained apart from the
installed toolchain and Resolve.
