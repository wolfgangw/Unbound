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
| Edit | Media Pool, Effects, Sound Library, Keyframe Editor, Edit Index, Inspector, Metadata, Mixer, Program and Source viewers |
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

The overlapping-squares button clicks to undock/dock and drags an already-floating
pane. Closing a host docks it. Strokes are muted grey (`#848484`) in both states,
brightening on hover; tooltips are "Undock" and "Click to dock; drag to move".
Dragging a docked button beyond Qt's standard drag threshold undocks through its
normal action and continues with the existing floating-host move/snap mechanism.
Below-threshold release remains a normal click; no hold timer is used.
`PaneUndockDrag` owns the gesture independently of the source button, preserving
it across header transfer/replacement for standard panes, native palettes and Edit
viewers. Retain the initial floating size, normalize saved maximized/fullscreen
state for dragging, and anchor the button's pressed point under the pointer.
Hosts stay transparent through initial show-time sizing until anchored, avoiding
a flash at remembered/centered placement. Shared widget events resolve deferred
targets with a bounded deadline; no polling or recurring discovery is introduced.
Raise the anchored host without activation during tearoff; activate only on successful
release while Resolve remains active and no modal dialog intervenes. This keeps
native activation/accessibility work out of sustained pointer movement without
suppressing accessibility notifications. Both platform movement backends accept
an optional activation choice; ordinary floating movement retains activation.
The shared window group gives a tearing-off host temporary priority below pinned
panes and above the focused ordinary window. Queued show/activation ordering must
retain that priority until release/cancellation; a one-time raise alone is undone
by the normal active-window-first policy. This state is neither pinning nor saved
layout data. Windows also includes it in the preliminary-raise guard.
An early release places the host at that release position without starting a
late drag or emitting another click. Escape, new press, application departure,
host hiding/closing and owner destruction release capture and cancel movement;
cancellation leaves an already-undocked pane floating. Ordinary floating-button
dragging is unchanged. As with the layout menu, confirm actual native application
departure before cancelling on Qt deactivation: native Scopes expansion/reparenting
can emit transient inactivity while Resolve retains foreground ownership.
Windows user validation confirms continuous tearoff movement, priority below
pinned panes and live Scopes throughout the gesture. Remaining platform and
interaction coverage is in the platform checklist.
Plain title text and unused native header background also drag. Exclude controls
and selectable/editable text; preserve immediate native dropdown behavior.
Fairlight buttons sit immediately left of the native options action (Create Group
for Groups), never in a fallback strip. Other panes may use the compact fallback.

Hosts retain native resize borders, shell identity and window commands; Windows
hosts have no title bar. Nonmodal gestures and deferred content layout preserve
playback responsiveness. Manual undocking disables the native toggle to reclaim
space; docking enables it, deferred until page return when inactive. Docking actions
are queued only for panes with a catalogued visibility action or tool selector.
Returning a pane without either action completes docking without a toggle request;
such panes must not leave layout/page transition readiness permanently pending.
Native toggles remain usable; automatic restoration honors saved choices after initialization.
Edit Inspector/Metadata initialization is serialized: finish native activation and
detachment of one before requesting the other, so queued sidebar deselection cannot
strand a pending restore. This ordering policy lives in the shared pane catalogue.
Retain hosts/content across page switches and reveal immediately; initial startup
restoration has a separate short fade.
After successful docking, destroy the empty host through deferred Qt deletion.
Do not cache empty hosts or their compositors to accelerate later undocking:
Unbound's low memory impact takes priority over that latency saving. Retention
across page switches applies only to panes that remain floating.

`panepagecontext.h` resolves a pane's logical page independently of its dock slot's
screen. Catalogued auxiliary windows (`UiEditPanelAux`, `UiColorPanelAux` and
`UiAudioPanelAux`) use the
main window's page root for floating visibility, geometry observation and page
identity. Native Single/Dual Screen moves therefore retain the same host/content;
closing the auxiliary container is not page departure. Discovery includes these
named auxiliary branches at startup and on construction/show events. No screen-mode
polling, forced host reveals or separate Dual Screen layout state is introduced.
This applies to all catalogued panes, including scoped Color controls. Edit viewer
discovery also retains the players container outside main's tree, and viewer
visibility follows the persistent Edit page when initialized in its auxiliary window.
Survival, usable content and subsequent docking after leaving Dual Screen are
user-validated on macOS for the tested panes. macOS cold-start layout restoration
in Dual Screen and pinned ordering against both native windows are also validated.
Edit Media Pool survival is validated on Windows.
Remaining compatibility checks are in `doc/issues/dual-screen-floating-panels.md`.

### Docking safeguard

After native layout settles, a bounded shared check verifies visible allocation,
including clipping by ancestors. A one-pixel collapsed allocation returns content
to its saved floating geometry without activation. Inactive pages defer this check;
respect intentional native hiding and unavailable viewers. Save actual floating
state without overwriting layout snapshots; incomplete recall can leave one dirty.

The affected button pulses between muted grey and playhead red until its own menu
opens, then stays muted. Tooltip/menu guidance remains until successful docking;
another failure restarts the pulse. Animate only while unacknowledged, visible and
the app is active. No modal warning or forced native slot widths.
Edit Media Pool can collapse when hidden before a portrait timeline activates
"Use optimized UI layouts for vertical video", reproducible without Unbound.
Its guidance suggests disabling that preference; other panels use general guidance.

### Activation and page focus

Activating main or a floating pane raises visible, non-minimized peers without
changing selected-window focus. Group activation is event-driven; dialogs and
newer focus choices take priority. Automatic reveals must not activate hosts:
set `WA_ShowWithoutActivating` before native creation on both backends. Manual
undocking explicitly activates the new host.

`panewindowgroup.h` owns application-local pinning and grouped minimization.
The per-pane `keepAtop` preference orders visible pinned hosts above ordinary
Resolve windows only while a member or its Unbound layout menu owns activation.
The group includes the top-level windows containing catalogued native auxiliary
pages on Edit, Color and Fairlight. Controller discovery registers those page
widgets; group membership resolves their current window, including Resolve's
unnamed shared secondary wrapper. Both platform backends use this shared membership
for ordering. Native secondary-window minimization remains Resolve-owned; Unbound
only conceals/restores its own hosts when main is minimized.
The main-owned layout menu retains its popup level and focus above the pinned
group; opening it, clean recall and renaming must not lift main above pinned panes
or change their relative order. General Always-on-top use is user-validated on
Windows and macOS, including macOS application switching, main minimize/restore
and native dialog/Unbound menu priority. Windows keeps hosts at their ordinary OS level.
Cocoa applies temporary native pin levels only while the Resolve group is active (see Cocoa windowing below).
Neither backend activates windows to reorder them or uses idle polling. Dialogs, popups,
other applications and newer focus choices retain priority. Windows additionally
checks the actual foreground HWND before ordering the group. Submit the final
pinned/selected/peer order directly, never demoting pins as an intermediate step.
Windows constrains preliminary ordinary/menu raises and nonactivating pinned-peer
raises in `WM_WINDOWPOSCHANGING` with `SWP_NOZORDER` while pins are visible, then
applies the complete normal-window order in one `DeferWindowPos` batch. Genuine
pinned-window activation remains native and updates shared MRU order. Changing
the main window's order without activation while an Unbound menu is visible is also
suppressed when a floating pane or that menu owns foreground, including with no
pinned panes. This prevents the menu's owner raise from briefly covering its
invoking pane; genuine main activation remains native. Changing
only `hwndInsertAfter`, or trusting its proposed position, can still expose peers
through native owner/owned ordering. Never insert normal hosts after the menu's
topmost handle or reorder the menu itself in that batch. Preserve native activation,
size and ownership flags; ignore unrelated windows, dialogs and inactive application state.

Pinned peers use most-recent activation order for the running session. Ordinary
activation preserves that order; newly restored hosts join in catalogue object-name
order. This order is not persisted and does not dirty layouts. Native ownership,
palette registration, keyboard routing and page visibility remain unchanged.
When main is minimized, temporarily conceal its visible, non-minimized hosts;
restore only that set on main restoration and only for available pages. Native
conceal/reveal preserves Qt visibility intent, maximized/fullscreen state and focus.
Windows uses nonactivating SetWindowPos visibility flags; Cocoa uses orderOut and
orderFront. Grouped hosts leave the taskbar while concealed. Independently hidden
or minimized panes remain so. No extra workspace window is introduced.

Panes without a usable remembered position open centered in the available work
area of the display containing their native pane's center (falling back to its
window's screen). Shared placement includes Edit viewers and accounts for final
show-time sizing. Existing remembered positions retain priority.
After showing a host and after saved-geometry restoration, shared recovery checks
the actual client allocation, including native minimum-size growth, against the
available work areas. Preserve fully visible spans across adjacent monitors;
otherwise fit to the work area with greatest overlap. If a native minimum exceeds
that area, keep it and anchor the top-left controls inside the work area. This
check runs at opening/restoration, not during user movement or resizing.

`pagefocus.h` remembers the last focused window/control per page for the running
session. After queued reveals, restore an available target or retain native main
focus. Ignore navigation/hide-show churn; cancel deferred recall on newer input,
app deactivation or superseding page changes. Dialogs retain priority. Focus memory
is neither settings nor layout data.

### Floating geometry restoration

`PaneGeometryTransaction` retains requested sizes for all standard pane hosts and
Edit viewers through initial saved placement, layout recall and page return.
Native content can temporarily raise minimum sizes during queued layout work;
Qt grows the host but does not shrink it when those minimums fall again. Reapply
saved placement when current native minimum/maximum constraints permit it, without
changing those constraints. Page return uses the outgoing working geometry,
including unsaved manual sizing, rather than the layout snapshot.

Observe host layout requests until one second of inactivity, with a three-second
absolute cap. New requests supersede old ones; host input, geometry gestures,
hiding, changed window state and docking cancel restoration. Only queued correction
work contributes to transition readiness, never idle observation of an unsatisfied
constraint. Persistent native constraints and monitor recovery can legitimately
prevent an exact match and leave a layout dirty. No polling or fixed size offsets.

`PaneMainResizeGeometry` preserves a floating host's intended placement across
main-window resizes. Resolve's responsive layout changes descendant header,
spacing and control minimums even while those descendants float; Qt can grow their
hosts without shrinking them when the minimums fall. Main Resize events start
the same bounded geometry transaction, retaining the pre-growth intention across
later main resizes and page returns. Explicit recalls replace that intention;
host input, native move/resize gestures and window-state changes discard it.
Native minimums remain authoritative. The dormant observer does no polling or
correction work between main resizes, and the intention is session-only. Both
standard panes and Edit viewers use this shared mechanism on both platforms.
Windows Color validation confirms exact saved pane positions and sizes after
startup, main maximize/restore and an Edit/Color page round trip. Larger native
minimums remain effective while main is maximized. Broader mode/layout combinations
and macOS validation remain in `doc/issues/color-controls.md`.

### Snapping

Main and floating windows snap during movement/resizing to each other's edges,
including alignment across gaps. Per axis, smallest adjustment wins; ties prefer
the nearer window. Duplicate edges produce one effective position. Work-area edges
require local perpendicular overlap; window edges win equal-adjustment ties.
Alt bypasses snapping. Zones are 24 logical pixels, DPI-scaled on Windows.

A 32 ms quadratic ease-in animates only snap capture/release corrections; pointer
movement stays direct. Frames run only during transitions (16 ms cadence).
Established locks follow proposals; release commits exact geometry. Cancellation
stops without committing, and Alt clears transitions immediately. Preserve native
constraints and measured contact bounds rather than adding arbitrary offsets.

Shift replaces all window/work-area edge targets with a display-wide grid during
move/resize. `panegridsnap.h` selects the work area under the pointer (nearest when
outside), anchors lines at its top-left, and spaces them 24 logical pixels apart.
Moves align left/top without changing size; resizes align only manipulated edges,
excluding positions outside native minimum/maximum sizes. If no valid grid line
exists, retain the native constrained position. A two-logical-pixel midpoint
hysteresis avoids jitter; line changes reuse the 32 ms accelerating transition.
Release Shift to return to ordinary snapping within the gesture; Alt overrides
both modes. Reset grid/transition state on mode changes and gesture boundaries.

`panegridguide.h` supplies a faint, nonactivating, input-transparent work-area grid
only during Shift gestures. It also draws unlabeled playhead-red outlines with 10-logical-pixel corner radii and a smooth
Gaussian alpha falloff (sigma 2, six-pixel reach on either side) from the exact edge centerline
for other pages' last working main-window and floating-pane placements, including
pages not yet visited. Combine identical rectangles; docked panes, current-page
windows and archived layout snapshots are excluded. These are visual references,
never magnetic targets. Query the controller's in-memory settings once when the
overlay opens, without file reads or layout changes. Shared recovery handles
missing displays; native main placement converts to logical contact coordinates
through the platform adapter. Use maximum per-pixel alpha, never additive blending, so shared/nearby edges
cannot intensify each other. Cache the DPI-scaled raster during a gesture, rebuild
only on reference/display changes, and free it after fade-out. Fade the entire overlay in/out over
100 ms on activation/release, including mouse release and Alt bypass. Reverse
interrupted fades from current opacity; hide on fade-out completion. Retain the
hidden overlay for reuse, refreshing references on activation; stop animation and
destroy it with its gesture owner. Animation runs only during fades.
Windows scales grid units with window DPI; Cocoa uses logical screen points.
Windows observes modifier changes through a gesture-scoped GUI-thread keyboard
hook that only posts a refresh and never consumes keys. Cocoa observes flags-changed
events. Neither backend polls modifiers while idle. Native macOS grid behavior
requires validation; shared calculations and Windows builds run in normal fixtures.

## Specialized pane mechanisms

### Edit viewers

Program and Source use normal header buttons and independent geometry/floating
intent. Source restoration waits for native availability; do not force Single/Dual
Viewer Mode or add a separate control window. When no visible viewer stays docked,
`viewergeometryadapter.h` expands the timeline using retained native baselines.

Floating Source uses the activation bridge described under Audition players;
native focus continues to select Source versus Program/timeline transport.
Bind before first host show and stop before docking. Program needs no bridge.

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

### Audition players

`auditionactivationbridge.h` serves Edit/Fairlight Sound Library, Fairlight Media
Pool and Edit Source viewer. Bind the recognized host/page/player before first show.
On genuine host activation, deliver one main-window activation notification to restore Resolve's
command context; consume that exact event before QWidget propagation and let
actual focus select the player. Do not forward transport keys, force focus or alter
waveforms. Stop before docking/reparenting; tolerate nested notification/destruction.
Sound Library's player is `m_PlayerPreview` and needs no Qt parent change.
Fairlight Media Pool's `previewPlayer` retains the tested Qt parent/native owner
relationship. Windows adds `WS_EX_APPWINDOW` before first show so the owned host
retains taskbar/Alt+Tab eligibility and grouping identity.

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

### Color Clips

Floating Clips uses native Lightbox rendering, thumbnail sizing, filtering,
timecode ruler and overflow scrolling. Minimum host size is 360 by 240 logical
pixels; Windows reflows on resize release. The floating button sits at the top-left
gutter corner, centered horizontally in the reserved 32 px leading gutter with a
4 px top inset. Docked placement retains its native thumbnail-view alignment.
Gutter and unused native header background drag; never cover clip decoration/native controls.

Before entering floating Clips' Lightbox mode, initialize the native Timeline in
normal Color mode and capture its constraints after the queued native layout pass.
Restore its prior toggle state before Lightbox entry. Hidden Timeline geometry can
be a collapsed strip and is not a valid native height baseline. Pending setup
cancels on docking and defers completion while its page is hidden.
`NativeToggleState` preserves the user's checked state across this deferred
initialization. Settings capture reads that intent and ignores temporary clicks;
ordinary signals and native handlers still run. Release the guard after restoring
the toggle, including cancellation. A transient initialization state must never
dirty a layout or replace its remembered visibility choice.

The shared adapter replaces empty Lightbox allocation with the main viewer and
optional Timeline, reserving the left Gallery/LUTs/Media Pool stack and right
Nodes/Effects containers according to native toggles and constraints. Transfer
existing Color controls into an additional row above the retained Lightbox toolbar,
inserted in its native vertical layout. Preserve connections and restore original
ownership/order on docking. Suppress interim repaint during native layout passes.

Initialize all other saved Color panes before entering Lightbox, including lazy,
not-yet-discovered panes. Keep retained Clips suspended across deferred discovery
and recall passes until their attempts finish. This guard also covers incoming
layouts without floating Clips: outgoing docking may restore Lightbox-on and hide
controls needed for incoming initialization.
Manual docking restores the pre-float Lightbox/Color Controls choices, deferred
when inactive. Color Dock all instead requests normal Color mode, using the
same playback lease and pre-docking Clips suspension as layout recall. Hold that
lease through queued discovery so native returns share one stop/resume; saved
layout snapshots remain unchanged. Windows and macOS layout recall → Dock all → recall
is user-validated with active playback, including the brief coordinated pause.
Native Lightbox exit docks Clips. Explicit recall of a Color layout
without floating Clips instead returns to normal Color mode, even for a clean
active layout. Do not restore outgoing Lightbox-on after that recall. Hosts retain
normal page visibility and geometry persistence; mode choices add no settings field.

`colorplaybackpause.h` provides optional, bounded transport assistance around
required Color Lightbox changes: Clips entry/return and retained-Clips suspension
through layout initialization. Nested leases share one stop; already-stopped
playback and clean floating-Clips recall need none. Discover standard transport
buttons in the docked or floating Color viewer by object name. Both direction
buttons unchecked means idle, even when Stop is also unchecked after startup;
do not issue transport commands or retain a resume lease for that state. For
active playback, click native Stop and require Stop checked plus both direction
buttons cleared before retaining automatic resume intent. Unknown, incomplete,
disabled or unacknowledged transport skips the workaround and continues normal
layout work. Playback assistance must never refuse recall, undocking or docking;
it returns only an optional lease, not permission to proceed. Diagnostic trace
markers record unavailable assistance without a user-facing error.
Release the final lease after deferred mode work; two queued continuations let
native handlers unwind before resuming only the playback this transaction stopped.
This is not a global renderer-ready signal. New input/transport commands,
page/project departure, application deactivation, dialogs, failure, shutdown and
a three-second deadline cancel automatic resume. No seeking, loop changes,
shortcut forwarding, private offsets or polling.

Read shuttle speed from the public QLabel `labelPlaySpeed`; explicitly hidden
text is stale and means normal speed. Resume with the original direction button,
then invoke the corresponding native QAction `controlPlayForward` or
`controlPlayReverse` for a bounded count of power-of-two speed steps. Label updates
are asynchronous: never feed its immediate post-command text back into that loop.
Unrecognized/unavailable speeds or actions fall back to 1x in the same direction.
These are public Qt operations on discovered native controls, not a documented
Resolve scripting transport API. Standalone live probes validate ordinary and
2x/4x shuttle restoration on Windows; integrated fresh-process validation and
macOS validation remain outstanding. Readiness limits remain in
`doc/issues/layout-transition-readiness.md`.

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

The Color viewer transfers `UiMainViewerForm` from `m_pFramePlayer` using the
shared native-layout-preserving session. Its outer `m_pFramePlayerContainer`
remains available to native layout and the Clips adapter. Header/status and
native tool-page backgrounds use `PaneHeaderDrag`. Windows floating playback
and interaction are user-validated; space reclamation remains deferred.

Lower Color controls include Camera Raw, Color Match, Color Wheels, HDR, RGB
Mixer, Motion Effects, Curves, ColorSlice, Color Warper, Qualifier, Window,
Tracker, Magic Mask, Blur, Key, Sizing, 3D, Keyframes, Scopes and Info. General
floating-control use is user-validated on Windows; 3D requires separate validation
with stereoscopic material. Controls are catalogued with their native stack-family
scope, header owner and selector. Scope disambiguates repeated names such as
`sizingTab`. Retain stack entries and indices; transfer their contents with
`SidebarFloatSession`. Camera Raw and HDR transfer the whole multi-entry layout;
other tools preserve the native layout and move its single content widget, except
Curves/Color Warper/Scopes (native expansion below) and Key/Sizing/Tracker. For these tools,
retain `keyWidget` (`UiSecKeyWidgetImp`) and `sizingWidget`, plus Tracker’s `trackerWidget`,
as the first widgets in their respective `keyTab`/`sizingTab`/`trackerTab` native layouts;
transfer each tool object's own layout contents instead. Resolve's
native tool lookup requires that container in its original slot and dereferences
these lookups without a null check. Keep its identity, stack index and native
connections intact throughout floating/docking; never replace it with a placeholder.
The placeholder belongs inside the retained tool container.
Floating Tracker page return, docking and layout recall are user-validated on Windows.

Curves and Color Warper use `ColorNativePalette`: activate the native Expand button
and retain its registered `UiFloatingPaletteWidgetDialog` as a child inside the
normal Unbound host. Discover the palette/dialog by names and ancestry. Resolve's
expanded-palette registration keeps histogram/image data updating when another
Color tool is selected; plain reparenting does not. While embedded, clear only the
expanded dialog's explicit minimum size; retain descendant constraints and layout
size hints so the controls determine the useful minimum width and height. Restore
the dialog minimum before native return, and relax it again if return is rejected.

For floating Warper only, `ActiveStackSize` makes `m_WidgetPanel` use the active
mode's native size requirements. QStackedLayout otherwise includes hidden modes,
whose bottom controls can impose a wider minimum on Chroma Warp. Temporarily use
Ignored size policies on inactive page containers, retaining all descendant
constraints; restore each page's policy when selected and all policies before
native return. Preserve newer native policy changes. Current-page signals and
coalesced child/layout events handle mode changes and late pages. Refresh layout
minimums through the host after policy changes; native layout grows for larger
modes, while shrinking remains the user's choice. Do not stretch the square
graphic, change the selected mode, or alter other Color controls.
Standard floating lower Color controls also use `ActiveStackSize` for native
`m_WidgetPanel` mode stacks, suppressing only inactive modes' vertical hints.
Retain the native horizontal budget across all modes; active content minimums
remain authoritative. Restore policies before docking. This avoids hidden
Qualifier modes imposing their taller height on HSL; compact HSL sizing is
user-validated on Windows.
Compact palette resizing and simultaneous updates are user-validated on Windows.
Curves' native histogram source remains the user's choice: Input shows the signal
entering the node; Output reflects adjustments within that node. Do not force
Output or treat an unchanged Input histogram during grading as an update failure.
Native expanded-state toolbar appearance, return actions and replacement-tool
selection remain Resolve's. Warper mode is clip/node dependent;
never overwrite it. Already-expanded palettes use the same host path. Docking
restores the dialog's native parent/flags and calls its public virtual
`QDialog::reject()` to return content and registration through Resolve. Reparenting
hides the dialog; `close()` on a hidden QDialog can succeed without calling reject
or returning the palette. Native Close/Escape requests defer normal session docking
until the event unwinds; rejected closes
retain a usable floating host. Page visibility, geometry, layouts, header dragging
and snapping reuse the standard session. No private offsets, update polling or
transport interception. Key's matte updates work through its existing retained
container and do not require native expansion.

Floating Scopes uses `ColorNativeScopes` through the same native-palette session.
Its independent renderer belongs to Resolve's `UiWaveformWindow`, separate from
the single docked scope whose updates depend on the lower-right tool selection.
Discover the named native window as a QWidget: macOS uses a QWidget shell,
while Windows uses a QDialog. Open it through `btnUndock`, or adopt it if already
open, and embed the whole window in the standard host. Keep the docked scope in
its original slot;
no placeholder or content transfer belongs there. Retain native single/dual/quad
view controls, settings and renderer registration. Clear only the dialog's explicit
window minimum; descendant constraints still determine useful sizes. The native
multi-scope mode can require more space than an older single-scope saved layout.
Insert the Unbound button before the native dialog options action and make unused
title background draggable. Native Close/Escape/Hide requests use deferred session
docking; page hiding retains the session. Docking restores parent, window flags,
geometry and minimum, then invokes the public `btnHide` action. No forced main
Scopes selection, frame copying, transport interception or update polling is used
to keep floating signal updates alive. Windows startup, page return, native
Close/Escape and independent updates with Info/Keyframes selected are validated.
macOS Scopes undocking, free resizing, view-count controls, live updates, redocking
and header painting after page return are user-validated.

`widgetquirks.h` attaches platform-specific work for the embedded Scopes lifetime.
The macOS quirk invalidates visible native `titleFrame`/`headerFrame` subtrees after
queued show/content-move layout work. Resolve can show embedded Scopes at its former
top-level position before layout moves it back inside the host; show-time header
updates are clipped at that interim position. Refresh again on the child Move,
without changing its geometry. Hide/reparent/destruction cancel deferred requests.
Focus, visibility and renderer state remain native. Windows needs no extra repaint.

Guard the retained docked Scopes subtree's Expand actions as well as the floating
dialog's controls; restore the original enabled states on docking.

`ColorScopesLayoutRefresh` refreshes the native view-count button availability
after coalesced size/show events. Resolve updates these buttons in its custom
border gesture rather than ordinary Qt resize delivery. Deliver a zero-distance
press/move/release directly through the dialog's public `QObject::event` virtual,
then restore its exact allocation and last non-resize cursor before returning to
the event loop. Track native non-resize cursor changes while attached; never
retain a temporary corner/edge cursor sampled after the user's gesture.
This retains native availability rules without imposing the native window's
aspect ratio. Direct widget delivery keeps synthetic input out of application
menu dismissal, focus and geometry-restore filters. Defer during real mouse input
until release; cancel on hiding and remove before docking. No idle timer, private
slot, address or size threshold is used. Windows resize, view availability and
open-menu recall are validated. While embedded, consume mouse gestures reaching
the native dialog itself so its original window move/resize handlers cannot resize
content independently of the host. Descendant controls and scope plots retain
their input, including native wheel panning, which can expose black space beyond
the plotted range. The refresh's direct widget dispatch bypasses this gesture
guard. Cursor cleanup and inner-border suppression are user-validated on Windows;
macOS validation remains in `doc/issues/color-controls.md`.

Insert buttons in existing header rows, before recognized options where present;
use the existing `PaneHeaderDrag` and floating hosts. Tool selectors are not
visibility toggles: never switch them off for reclamation. Explicit docking
selects the native tool, deferred until page return. While a tool floats, disable
recognized native Expand buttons and restore their original state on docking.
`ColorExpandGuard` observes only that floating subtree: enforce disabling on native
enabled/show changes and coalesce child-addition discovery for late header variants.
Remove the guard before returning content, preserving explicit disabled state.
Native expansion while docked is unchanged. Header decoration extends to newly
created variants on bounded discovery events, including Scopes. Nested palette
selectors place a button before each native options variant and mirror its explicit
visibility; their plain child frames reuse `PaneHeaderDrag`, excluding controls.
Sizing's `cbWidgets`, Tracker's `cbBoxTrackStableSelection`, Blur/3D's `cmbSetTo`
and Color Wheels' `cbColorWidgets` selectors use that same child-frame handling.
Floating Color Wheels ignores the wheel stack's horizontal size hint, whose native
child minimums follow the last expanded allocation. A stable minimum is derived
from the widest mode's intrinsic group hints plus the tightest native mode's
spacing budget. Bars can retain expanded gaps/margins as well as allocated group
minimums; those must not inflate the permanent floating minimum. Child constraints
and native padding remain unchanged; docking restores the original stack policy
and minimum. Remaining validation is tracked in
`doc/issues/color-controls.md`. No lower-region reclamation yet.

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

The final entry, separated below the layout list/status, is the checkmarked
`Always on top` preference for the invoking pane, including Edit viewers.
It is disabled while docked; docking retains the preference for the next undock.
Changes persist in working state even without a layout, participate in snapshots
and dirty detection, and never overwrite a saved layout implicitly. Missing means
false; unpinning removes the optional field so legacy snapshots become clean again.
This is an additive format-7 field: loading pre-feature files does not rewrite them
or modify saved layouts. First use changes only the requested working pane; old
layout recall clears the preference where absent. Existing format 1-6 migrations
remain unchanged, and settings writes retain their atomic-save safeguards.

Right-click any Unbound button for up to five layouts per page. "Save as new Edit
layout" (using the catalogue's current page label) creates a snapshot; identical
configurations do not create duplicates. Only explicit update overwrites a snapshot.
Active identity remains highlighted after edits; dirty layouts offer a save icon
left of trash. Switching layouts discards outgoing unsaved changes; no draft copies.
Clean active recall is a no-op after capture, except the Color mode rule above;
dirty recall restores saved state. Rename preserves identity and dirty state.

Dock all affects only the current page: dock hosts, cancel pending floating intent,
clear active layout identity, preserve saved layouts/main placement/other pages.
Keep its row visible and disable it unless an actual current-page host exists,
including viewers; saved pending intent alone does not qualify. The temporary
zero-host interval during recall changes enablement only, never the menu's row
positions or height.

Use a main-owned Qt tool QFrame outside Qt's transient popup stack so native docking
cannot dismiss/reopen it. Anchor below the invoking button, clamped to the work
area. Retain rows keyed by UUID, update styles in place and coalesce refreshes.
Save/recall/update/delete stay open. Outside interaction (including scrolling,
keyboard input and dragging), Escape and actual native application departure dismiss;
hover alone does not. Transient Qt ApplicationDeactivate events during native
reparenting do not dismiss it while the OS still considers Resolve active. A
menu-lifetime native observer handles real departure even when Qt activation
notifications are delayed. Windows uses foreground-window events; Cocoa uses
application-resignation notifications alongside native menu tracking. Growing
status rows are clamped back into the work area without hiding/reopening the menu. The application filter exists only while visible.
The menu owns explicit open/dismiss intent through the public QWidget visibility
override. Native tool-window hide/show sweeps cannot flash it during palette
activation or reopen it after dismissal. Outside input, Escape, actual application
departure, close and owner hiding/minimization use explicit dismissal; destruction
retains ordinary Qt ownership. This does not filter other windows' activation events.
Backends emit `WindowInteractionEvent` before user move/resize starts, including
native-child edges; programmatic recall geometry does not emit it.

Use main-window background palette, 4 px popup corners, 3 px highlight corners,
5 px row-control gaps and 2 px vertical margins. Layout-name highlights, rename
fields, dirty-save indicators and trash hover highlights share a 30-logical-pixel
height with no theme-dependent vertical padding. Dirty save uses playhead red;
trash stays neutral. Preserve active highlighting through hover/focus.

Hold a name for one second to edit inline; activate the popout, focus the editor
and select its text. The precise single-shot timer cancels on release, leaving,
hiding or movement beyond the drag threshold; completed hold suppresses recall.
Enter renames only, including inactive rows; Escape/focus loss cancels. Names are
trimmed, nonblank, at most 128 characters and may repeat. Use UUIDs for all actions.
While the editor owns focus, hand off only this popout's key/shortcut events directly
to its Qt event handler: sendEvent would re-enter Resolve's swallowing route.
Unmodified Left/Right collapse selection to start/end; modified keys stay native.
Do not synthesize clicks or change global shortcuts; clear callbacks on teardown.

### Layout transition presentation

`layoutrestorepolicy.h` is the shared entry point for page/pane restoration
exceptions. It selects the existing Color playback assistance, ordinary Color
mode, expected native palette dialogs and bounded geometry-settlement behavior,
and exposes temporary native-toggle intent to adapters and settings capture.
The underlying helpers retain native ownership and public Qt interactions; the
policy contains no platform APIs. Add diagnosed exceptions here, not in capture
backends or per-page menu copies. A persistent native minimum remains a real
restoration constraint: do not override it, mask the resulting layout difference,
or silently rewrite an older saved layout.

`layouttransition.h` owns the shared `LayoutTransition` lifecycle: begin,
readiness/invalidation, reveal and cancellation. The platform contract
`layouttransitionpresentation.h` supplies optional capture/presentation, retained
buffer accounting and actual application-departure notification. A failed or
unavailable backend leaves normal layout work running without a visual wait.
Backends must release temporary resources on completion/cancellation and never
request capture permissions. Windows implements the current desktop snapshot
cover. macOS explicitly uses `NoLayoutTransitionPresentation` for this optional
feature; existing pane/layout behavior is unchanged. Alternative presentation
mechanisms can implement the same contract without changing shared pane policy.

The Windows internal `-LayoutCoverExperiment` captures all displays through
public GDI before showing nonactivating covers. External video appears briefly
frozen while playback continues. No images are saved. Raw bitmap storage is capped
at 128 MiB; native compositor memory is additional. Completion/cancellation frees
the captures, and docking releases empty hosts rather than caching compositors.
Reveal uses a 120 ms fade. Input outside the live menu, real dialogs, application
departure and superseding transitions cancel protection. A three-second GUI-thread
deadline releases it on failure but cannot preempt a blocked GUI thread.

Readiness combines tracked restoration work, 50 ms structural quiet and final
coalesced updates, with at most three update attempts. Public GL composition must
match the window's current geometry revision; raster acknowledgement follows
UpdateRequest delivery. Windows flushes DWM before fading. This is bounded
presentation evidence, not proof that every native renderer has finished. Paint
activity alone does not prevent reveal; idle observers do not impose a wait.

Callers may supply existing popup-level interactive windows. They remain live
above the presentation, and their input does not cancel it; hiding/destroying one
cancels its transition. The layout menu uses this path. Windows temporarily
excludes those windows from capture with public SetWindowDisplayAffinity, restores
each previous affinity immediately after capture on every path, and inserts covers
below the existing popups without activation. No hide/capture/reopen cycle or
frozen menu copy is used. Capture exclusion requires Windows 10 version 2004 or
newer; unavailable exclusion skips visual protection, never layout work. Normal
menu painting/input and shared sizing continue during the transition; synchronous
GUI-thread layout work remains synchronous.

The opt-in `LayoutTransitionRecall` adapter supplies experimental readiness for
layout recall on every supported page, using shared work/geometry/composition
evidence. Edit viewer restoration registers its queued work; unavailable Source
intent does not hold a transition open until the viewer becomes available.
Keep page-specific native workarounds in their adapters; the Color playback lease
is confined to Color recalls. Keep the readiness adapter
separate from the reusable presentation lifecycle; its signals are not a general
native-rendering completion guarantee. The open experiment and remaining
validation are in `doc/issues/layout-transition-readiness.md`.

The same opt-in build includes `pagetransition.h` for native page switches.
It observes the named page stack without intercepting page shortcuts or changing
selection. Cover when either page has floating content/intent. An outgoing page
with floaters captures at its Hide boundary before pane-session object filters
hide hosts; otherwise an incoming floating page captures at its Show boundary
before hosts return. Carry the departure cover into the destination without
recapture. Duplicate Hide notifications are coalesced. Two fully docked pages
allocate no cover or readiness trace. A docked destination uses its own readiness
evidence and does not wait for outgoing hosts or other pages' restoration work.
Readiness includes incoming queued host reveals, geometry corrections and main
placement; departure from that destination cancels the transition.
Public event timing is provisional evidence: the instrumented build counts main
paint/update delivery before arrival capture when needed, which does not prove
absence of earlier native/GPU presentation. Keep the live capture-point validation
in the open investigation. Page switches acquire no Color playback lease and add
no native layout mutations. The common cancellation/deadline rules still apply.

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
viewer parenting changes. Docking must run the viewer-region restoration path even
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
`WStackPage_Audio`. The repository is otherwise self-contained apart from the
installed toolchain and Resolve.

Integrated recall diagnostics are opt-in through `UNBOUND_LAYOUT_TRACE` (Windows
internal build switch `-LayoutTrace`). `layouttracediagnostics.h` observes on the GUI
thread for six seconds per explicit recall, buffering bounded phase milestones
separately from numeric structural/paint/public-GL-swap aggregates. Hot event paths
do not build JSON; bursts cannot evict phase timings or terminal status. Trace-only mode does not
consume input, suppress painting or delay recall; a quiet candidate is diagnostic
evidence, never a presentation-ready guarantee. Diagnostic work markers separate
queued corrections/checks from idle observer lifetimes; window geometry revisions
qualify public GL composition evidence. Raster/native surfaces remain unproven.
The separate opt-in Windows layout cover prototype and its readiness limits remain
an open investigation in `doc/issues/layout-transition-readiness.md`.
Build/use and trace summarization
are documented in `tests/platforms/windows/viewer-probe/README.md`.
