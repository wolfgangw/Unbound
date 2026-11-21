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

Windows x64 builds are unsigned. Universal macOS builds (arm64/x86_64) are
ad-hoc signed without Developer ID or notarization. State publicly that native
Intel Mac operation remains unvalidated and is offered for community testing.

| Page | Supported content |
| --- | --- |
| Edit | Media Pool, Effects, Sound Library, Keyframe Editor, Edit Index, Inspector, Metadata, Mixer, Source and Program viewers, floating Video Scopes |
| Color | Nodes, Gallery, LUTs, Media Pool, Effects, Timeline, Clips, Viewer and lower Color controls |
| Fairlight | Media Pool, Effects, Index, Groups, Sound Library, ADR, Mixer, Metadata and Inspector. Meters are excluded. |

Use **layout** in the UI, code and tests. Retain compatibility wire keys and
Resolve-owned names. The [pane catalogue](src/core/panecatalogue.h) is the list
of supported controls, discovery names, header anchors, toggles and settings keys.

## Architecture

Resolve owns the content. Unbound owns hosts, placeholders and decoration.
Docking restores ownership, layout slot, constraints and native state.
`src/core/` uses contracts in `src/platform/` for windowing, events, placement,
DPI conversion and data directories. CMake selects one `src/platforms/<platform>/`
backend. Narrow native workarounds belong in its `quirks/`.

| Start here | Responsibility |
| --- | --- |
| [Controller](src/core/controller.cpp) and [catalogue](src/core/panecatalogue.h) | Discovery, lazy initialization, pane registration, native toggle tracking and deferred restoration |
| [Pane sessions](src/core/panefloatsession.h) and [sidebar sessions](src/core/sidebarfloatsession.h) | Reversible native layout transfer and host lifetime |
| [Decoration](src/core/panedecoration.h), [header dragging](src/core/paneheaderdrag.h) and [window group](src/core/panewindowgroup.h) | User gestures, group visibility, activation and stacking |
| [Page context](src/core/panepagecontext.h) and [restore policy](src/core/layoutrestorepolicy.h) | Logical page ownership and diagnosed pane/page exceptions across transitions |
| [Settings](src/core/panesettings.h), [geometry](src/core/geometryjson.h) and [main placement](src/core/mainwindowplacement.h) | Shared persistence, coordinate representations and monitor recovery |

Refresh Color headers only on the visible Color page, including page return.
Unrelated child creation must not search inactive controls. Clips reconciliation
follows its cross-page ownership.

`unbound_link_core()` in [core CMake](src/core/CMakeLists.txt) supplies the same
production sources to both runtimes and controller fixtures. Shared pane fixture
targets live in [targets.cmake](tests/core/pane-float/targets.cmake). Platform
fixtures supply the native operations through their layout adapters.

## Startup and installation

Install into the current account with Resolve closed. The per-user Fusion Scripts
root contains the startup hook `Unbound.scriptlib`. Managed menu scripts belong
in `Scripts/Utility/Unbound`. Leave other Utility scripts alone and move obsolete
launchers outside the Scripts scan tree. The private runtime lives under
`%LOCALAPPDATA%/Unbound/Runtime` on Windows and
`~/Library/Application Support/Unbound/Runtime` on macOS.

Resolve evaluates script libraries in several Lua contexts. The hook requires
usable global FFI and checks process identity through public OS APIs before
loading native code. External Workspace script hosts skip startup silently.
The Qt-free `UnboundLuaLoader` validates the host and queues attachment of
`Unbound.dll` or `Unbound.dylib` on the Qt GUI thread. Repeated startup reuses
one controller. Use Resolve's Qt runtime. The installer must never re-sign
Resolve, start or terminate it, or install an external service.

Install/update/uninstall preserve settings and layouts. Replace runtime and
commands transactionally, rolling back on failure. Omit stale `UnboundHelper.dylib`
from Mac payloads. [Installer wording](src/installer/dialog.json) and `presentation.h`
are shared. A matching intact installation shows disabled Install and enabled
Uninstall. Repair requires a damaged matching payload. A newer installation
disables both actions and points to the newer installer. Compare semantic
versions first, then internal builds of the same base version.

### Script commands

[commands.json](src/scripts/commands.json) defines the stable filenames, command
IDs and arguments. The [generator](infrastructure/scripts/generate.py) produces
the eight entries under Resolve's Workspace > Scripts > Unbound. Users assign
shortcuts through Resolve's Keyboard Customization, with native conflict handling.

- **Studio and unknown edition.** Workspace scripts execute outside Resolve.
  They use the existing Fusion scripting connection and `fusion:Execute(...)` to
  call `UnboundExecuteCommand(version, command, argumentsJson)` inside the
  already-loaded runtime.
- **Confirmed Free.** Startup queries `IsStudio()` once and passes the edition to
  the loader. Once attached, `ScriptCommandFallback` listens to the managed
  submenu's native `QAction::triggered` signals. Only discovery of the named
  Workspace root is application-wide. Follow its menu links, reconnect rebuilt
  actions once and drop removed actions. Free's Lua payloads return silently to
  avoid double delivery. This is command delivery after startup.

Both script routes use the versioned C ABI and share GUI-thread operations with
the menu. Copy arguments before returning and validate command IDs and JSON.
`LayoutCommandGate` admits one request at a time. Reject reentrant requests,
suppress execution during dialogs, popups, minimization or actual app departure,
and discard queued work invalidated by a page change or newer menu operation.
Count pending restore work on both platforms and bound the wait for settlement.
Never replay a burst of shortcuts or install a global key handler.

## Pane lifecycle and interaction

The Unbound button clicks to undock/dock and drags to move. A docked button
starts tearoff after Qt's drag threshold. Anchor the pressed point to the pointer
at the initial floating size. `PaneUndockDrag` owns the gesture across header or
button replacement. Raise the host without activation while dragging, above
ordinary windows and below pins. Activate on release if Resolve is still active
and no modal intervened. Cancel on actual app departure, Escape, owner loss or
interruption. Preserve native accessibility and input delivery.

Decorate existing headers. Drag from titles and unused background. Controls
and selectable text keep native input. Decorate both populated and empty
Edit/Fairlight Metadata headers so the button and dragging survive selection changes.

Manual undocking turns off the corresponding native visibility toggle. Docking
turns it on, deferred on an inactive page. Queue native actions only for panes
with a known toggle or selector. Preserve saved choices across lazy initialization.
Serialize Edit Inspector/Metadata initialization so a queued native sidebar
deselection cannot strand the other pane. Keep occupied hosts on page switches.
After successful docking, delete empty hosts and their compositors.

Associate panes with their **logical page**, including named auxiliary pages in
Dual Screen mode. A pane changing top-level owner or an auxiliary window closing
does not by itself mean the user left the page. Follow the Edit players container
even outside main's tree. Preserve intent through Single/Dual Screen changes
without forced reveals or separate screen-mode layouts.

### Docking safeguard

A newly docked pane can momentarily receive a one-pixel allocation before
siblings and main placement settle. Observe geometry/layout in its ancestor
chain. Accept a usable allocation after a quiet interval. If it remains
collapsed at the bounded deadline, return the content to its remembered float
without activation. Respect intentionally hidden panes, unavailable viewers and
inactive pages. Save actual working state and leave the requested snapshot intact.
The affected button pulses red until its menu opens. Keep docking guidance there
until docking succeeds. Avoid forcing native slot dimensions.

### Activation and page focus

Automatic reveal sets `WA_ShowWithoutActivating` before native creation. Manual
undocking activates. `PaneWindowGroup` raises peers without taking focus. Within
active Resolve, pins stay above both Dual Screen owners in most-recent activation
order, with the menu above pins. Respect dialogs and app departure. On main
minimization, conceal visible, non-minimized hosts. Restore only that set on
available pages without activation.

[Page focus](src/core/pagefocus.h) remembers the last window/control per page
for the session. Restore an available target after queued reveals. Newer input,
a dialog, app departure or another page change takes precedence. A page change,
a Qt focus change and Resolve command activation serve different purposes.

### Floating geometry restoration

Center a new float in the work area containing its native center. Prefer its
remembered geometry thereafter. Check actual client bounds after show, including
native minimum growth. Preserve reachable positions spanning adjacent monitors
and keep captionless controls reachable if display configuration changes.

Native layouts can briefly raise their minimum size and leave a host too tall
after the minimum falls again. `PaneGeometryTransaction` holds the requested
rectangle across bounded layout settlement and reapplies it when constraints
permit. Explicit recall uses the snapshot. Page return uses outgoing **working**
geometry, including unsaved edits. Main-window resize preserves pre-growth
geometry through the same mechanism. New user input, move/resize, hide, state
changes or docking supersede corrections. Never counter a persistent native
minimum with a fixed offset or hide the discrepancy in dirty detection.

### Snapping

Snap main and floating windows to measured contact edges, nearest per axis.
Shift snaps to the pointer display's 24 logical pixel grid. Alt bypasses snapping.
Honor native constraints and animate only snap capture/release. Other pages'
working placements are visual references only. Scope modifier tracking and
the input-transparent guide to gestures and fades.

## Specialized pane mechanisms

### Edit viewers

`editviewers.h` gives Source and Program independent hosts, geometry and floating
intent. Follow Resolve's native Source availability and Single/Dual Viewer Mode.
In Single Viewer Mode, Media Pool hover can swap Program for Source. Keep a
floating Program visible through that swap and remember native visibility for
docking. Page/host hiding still wins. Leave preview, rendering and transport to
Resolve. Both floating viewers use the command activation bridge.

When no visible viewer stays docked, `viewergeometryadapter.h` expands the Edit
timeline from native baselines. Coalesce interim region allocations and defer
main backing-store presentation until the final geometry is applied. Disabling
child paints alone still allows an intermediate frame with misplaced clips.
Preserve already-disabled updates and keep floating viewers painting. Restore
native regions on docking, including after Cinema Viewer exit. Leave the native
video/audio divider ratio alone.

### Shared native Scopes on Edit and Color

Resolve has one globally registered floating `UiWaveformWindow`. Keep that
registration so Scopes update independently of the docked Color tool. Share
`NativeScopesContent` for exclusive ownership. Scope count, types and settings
are native global state. Window visibility, geometry and layout membership are
page-specific. Discover the native shell as QWidget on both platforms.

- **Edit** has no dock slot. [Edit Scopes](src/core/editscopes.h) adopts the
  window directly when opened through Workspace > Video Scopes. Explicit On
  brings the host forward. Startup, layout recall and page return are
  nonactivating. Host close, Unbound docking, Workspace Off and Dock all close
  it. Older layouts with no Scopes entry recall it closed.
- **Color** embeds the native floating window while leaving the single docked
  scope in place. Keep native view-count controls and plot input. Decorate the
  outer header once. Prevent the embedded shell from resizing independently
  of its host.
- **Cinema Viewer** may hide the native Scopes shell before hiding the workspace.
  Preserve floating intent and keep the shell visible on a separate display.
  Resolve supplies the signal. See the [paused Color Cinema limitation](doc/issues/color-viewer.md).
- **Sizing on Edit and Color.** Scopes can restore its former native size on
  show. Reapply the host layout before exposure if the embedded window no
  longer fills it. Let Qt deliver the normal paint. Keep the native view-count
  refresh for size changes and reuse its result on same-size reveals. Returning
  from Cinema Viewer with resized Color Scopes is validated on Windows.
- **Page handoff** must close the borrowed native renderer **while visible**,
  restore native ownership and remembered placement, then hide the outgoing
  host. Otherwise Resolve can reopen Scopes on the destination page or lose
  its position. Retain each page's intent for reacquisition. Run handoff after
  outgoing transition capture so Scopes stays in the captured layout. Main
  minimization alone is not page departure.

### Pane command activation and audition players

Qt focus on the selected child does not always restore Resolve's command context.
On genuine floating-host activation, [PaneActivationBridge](src/core/paneactivationbridge.h)
delivers one main-window `WindowActivate` through application notify, then
consumes that exact event before QWidget propagation. Native focus continues to
choose the subcontrol and transport. Never forward keys or synthesize a click.
Bind before first show, gate on real host activation and visible page/content,
and stop before docking. Popups, dialogs and newer focus choices take priority.

Catalogue scope covers Media Pool on all supported pages, Edit Keyframe Editor,
Color Nodes/Gallery/Timeline/Keyframes, Edit/Fairlight Sound Library and both
Edit viewers. Program needs the page command context even when Source is docked.
Local text editing alone does not justify enabling the bridge elsewhere.

Sound Library also has a separate audition audio player. On Edit/Fairlight,
`AuditionPlayerActivation` briefly focuses the native preview after host
activation settles, then restores the selected control if focus is still ours
to restore. Preserve field/list selection and transport position. Header focus
does not imply result-list focus. Fairlight Media Pool's preview player retains
its native owner, with Windows taskbar identity set before first show.

### Color Timeline and Clips

Float Timeline independently of Clips. Its floating header carries the title,
Unbound button and dragging, leaving the native ruler and track gutter available
for playhead input. Return the button to its original corner before docking.

**Color and Fusion share the same Clips widget** (`m_pLightBox`). On Color
departure return it to its native layout and remove Color decoration. Retain
Color's host, geometry and intent. On return, transfer it back before revealing
the host. Inactive docking or graph recovery must respect the current owner.

Floating Clips uses native Lightbox rendering, filtering, sizing and scrolling.
The button sits in the top-left gutter. The adapter allocates remaining space
to Viewer, Timeline and visible sidebars, and transfers the Color controls to a
row above the native toolbar. Restore toolbar ownership/order on docking.

Initialize incoming Color panes and capture a usable Timeline baseline in normal
Color mode **before** entering Lightbox. Preserve native toggle intent across
temporary initialization so settings and dirty detection stay accurate. Manual
docking restores pre-float mode choices. Native Lightbox exit docks Clips.
Explicit recall without floating Clips and Dock all request normal Color mode.

When a Lightbox mode change needs playback stopped, `colorplaybackpause.h` shares
one bounded playback lease across nested operations. Detect direction through
native transport controls. Resume only playback the lease verifiably stopped.
Keep direction and loop state, read speed from the public UI, and fall back to
normal speed in that direction if necessary. New input, lost context, a dialog
or expiry cancels resume. Unavailable transport must never block docking or recall.

Lightbox lacks normal sidebar divider sizing. For recognized dividers, temporarily
restore normal Color mode under a visual cover, deliver the native gesture in
order, then restore Lightbox, allocation and updates. Widths stay native session
state. Windows uses the Qt backing store. Cocoa needs a composited image of the
affected process windows to cover Metal surfaces. The scoped capture requires
no screen-recording consent. Failure preserves the existing layout. This is
separate from layout-switch presentation.

### Color viewer and lower controls

Keep the outer Color viewer container in the native layout when transferring
the player. Preserve the lower tool regions and their allocation. Tool selectors
choose content and must not be switched off as visibility toggles. Preserve
checked selectors across page departure **before Scopes handoff changes them**.
On return, reselect through native actions after queued page work. Newer input or
explicit layout operations supersede this session-only restoration. Never assume
a fixed number of tool regions.

| Tool or family | Native behavior the floating implementation must preserve |
| --- | --- |
| Ordinary tool stacks | Preserve stack order and indices. Camera Raw and HDR use a multi-entry layout. Where the catalogue specifies `retainedContainer`, keep the named native container first in its original tab layout and transfer only its contents. Resolve looks it up there. |
| Qualifier, Window, Tracker and Magic Mask | Qt focus alone does not assign viewer gestures. `ColorToolActivation` invokes the native tool selector on host activation or local press before picker input. A checked selector alone does not establish ownership after another tool used the viewer. The tray showing this floating tool's placeholder is the accepted tradeoff. Mere reveal, tearoff and Unbound-button input must not take tool selection. |
| Primaries black point, white point and white balance | [PrimariesPickerActivation](src/core/primariespickeractivation.h) activates the Color viewer's actual window, then completes the user's picker-button release once. Resolve arms the picker for that window and receives the sample. This requires neither a native tray switch nor interception of viewer strokes. |
| Curves and Color Warper | `ColorNativePalette` invokes native Expand and embeds the registered palette dialog. This keeps both updating independently. Relax its explicit window minimum only. Restore flags, ownership and minimum on native return, routing close/Escape through docking. Keep native toolbar state, histogram source and Warper mode. |
| Tools with multiple sizes | `ActiveStackSize` uses active-mode hints so hidden modes cannot impose a persistent minimum. Relax both axes for Warper and only height for ordinary tools. Color Wheels derives width from intrinsic group requirements. Preserve child constraints and restore original policies on docking. |
| Scopes | Use the shared global renderer and page handoff described above. Keep Resolve's native multi-scope interface and embedded plot interaction. |

Decorate existing headers and follow their native selection-state visibility.
Guard recognized native Expand actions only while floating. Restore original
enabled states on return. Discover replacement selectors on relevant input or
structural events. Keep paint and idle paths free of discovery. Remaining
mode, material and platform checks are in [Color controls](doc/issues/color-controls.md).

### Other pane constraints

Relax only catalogued, natively middle-eliding Inspector and Metadata labels.
Preserve native text, painting and other minimums. Restore policies on docking
without overwriting a newer native policy. Edit/Fairlight Index tab backgrounds
can drag, while tabs and actions keep their input.

## Layout menu

Menu and script operations share these per-page rules.

- Display five fixed numbered slots, with blank disabled names for empty slots.
  **Save as new always creates a distinct layout in the lowest free slot**, even
  when the configuration matches. Save updates the active layout. Renaming keeps
  its UUID and slot. Names may repeat. Deleting a layout leaves its slot empty.
- Recall captures pending working geometry/toggles, discards unsaved outgoing
  edits, and retains hosts that stay floating. Restore native toggles off-first,
  then on after float initialization. Dirty means actual configuration differs
  from the saved snapshot, including legitimate minimum-size differences.
- Dock all docks the current page, cancels pending float intent and clears its
  active layout identity. Keep snapshots, main placement and other pages. Its row
  is always present and enabled only with an actual host. The final Always on
  top row stores the pane's preference, including when no layout is active.
- Successful saves pulse visible buttons green. Empty-slot recall and full-slot
  save-as pulse red. Successful recall, unsupported pages, no active layout and
  busy requests are silent. A docking warning takes precedence. Keep docking
  guidance in the menu and transient action text out of it.

Right-click opens the main-owned tool window outside Qt's transient popup stack.
Keep it anchored and interactive during recall. Hold to rename in place and
scope editing keys to its editor. Outside clicks dismiss and reach Resolve.
Escape, app departure and owner hiding/minimizing also dismiss. Leaving starts
a two-second timer, cancelled on re-entry. Ignore transient Qt deactivation.

### Layout transition presentation

`LayoutTransition` owns begin, readiness, reveal and cancellation. The platform
presentation contract supplies optional capture and display. Layout work must
proceed immediately if capture fails or is unavailable. Never request screenshot
permission. macOS keeps native presentation and installs no readiness observer.
Windows release and internal builds use nonactivating GDI covers with a short
fade. Keep screenshots in memory, cap raw bitmaps at 128 MiB and release them on
every exit. Exclude the actual menu from capture and keep it interactive above
covers. Restore prior display affinity even on failure.

On page switch, cover if **either** page has floating panes or intent. Capture
before outgoing pane filters hide hosts, or on incoming show before restoration.
Reuse the departure capture. Fully docked-to-docked switches allocate no cover
or observer. Page transitions observe the destination's work and never acquire
a Color playback lease.

Readiness combines settled queued work, a short structurally quiet interval and
final update acknowledgements. Match GL composition to the latest geometry
revision. Raster confirmation follows update delivery. Windows flushes DWM before
reveal. This is bounded evidence of readiness. A deadline cannot preempt a
blocked GUI thread. Unavailable Source and idle paint observers do not hold up
reveal. Outside input, real dialogs, app departure or a superseding transition
cancel. Nonmodal native Scopes handoff is expected. See [open transition
checks](doc/issues/layout-transition-readiness.md).

## Settings reference

[PaneSettings](src/core/panesettings.h) owns per-user, cross-project settings.
Use `%LOCALAPPDATA%/Unbound/Unbound.json` or
`~/Library/Application Support/Unbound/Unbound.json`. `UNBOUND_SETTINGS_FILE`
overrides either path. Edit settings only with Resolve closed. Write atomically,
preserve unsupported/corrupt files and reject malformed layouts.

Format 8 migration assigns slots in existing order and preserves UUIDs,
snapshots, active identity, arrangements and name counters. Before migrating a
file with layouts, preserve its original bytes as `<settings-file>.pre-v8`.
Never overwrite that backup. Backup failure stops migration. Older versions
reject format 8. Keep existing wire keys, including `presets` and `activePreset`.

Separate per-page working state from saved snapshots. Persist native main
placement, pane floating intent, normal geometry, pinning and applicable native
toggle choices. Missing `keepAtop` means false. Exclude internal dividers,
viewer mode, minimized state, page focus, docking-warning acknowledgement and
temporary Lightbox choices. Flush pending geometry on docking/shutdown and
preserve floating intent across shutdown.

Pane rectangles use logical client coordinates, including valid negative desktop
positions. Retain normal geometry beneath maximized/fullscreen state. Windows
main placement uses native workspace coordinates with a Qt fallback for changed
monitors. Cocoa converts native frame/content rectangles to top-left logical
coordinates and preserves normal placement across zoom/Spaces. Allow the normal
frame to settle before reapplying saved placement. Compare actual geometry
without a fixed offset or tolerance that would mask an error.

## Build and validate

Internal builds keep `VERSION` and use a fresh `yyyy.mm.dd.NNN` runtime build
ID. Deliver `Unbound <version> (<build>).exe` under `release/win/<build>/` or
the corresponding `.app` under `release/mac/<build>/`. Use the runtime build ID
even when packaging has its own counter.

Explicit public builds go in `release/packages/`. Windows delivers
`Unbound-<version>-windows-x64.exe`. Mac distribution uses the basename
`Unbound-<version>-macos-universal` with the applicable suffix. An archived
Mac installer is `Unbound <version>.app` at the archive root. Staging output
is intermediate. Verify versions, payload hashes and macOS signatures/slices.
Do not overwrite existing output. Tagging, ZIP creation and publication each
require an explicit request.

### Windows

Run `infrastructure/windows/build-release.cmd -InternalBuild` from the repo root
for development. It builds the runtime and installer, runs shared and Windows
fixtures, and audits dependencies. The [Windows build README](infrastructure/windows/README.md)
covers prerequisites, release staging and the narrowly allowed Windows 10
menu-exclusion fixture result. All other failures remain fatal.

### macOS

Run `infrastructure/macos/build.sh` for a tested, audited and signed runtime
candidate, then `infrastructure/macos/installer/build.sh <absolute-candidate>`
for the self-contained installer. Use [macOS infrastructure](infrastructure/macos/README.md)
and its [installer procedure](infrastructure/macos/installer/README.md) for
staging and validation. Isolate fixture logs with `UNBOUND_LOG_FILE`.

## Platform implementation and validation

Both backends use Resolve's Qt 5.15.2. Validate native interactions in a fresh
Resolve process on the target platform. Rosetta execution does not validate
native Intel operation. Windows Free and broader mode/material coverage remain
open. See [platform validation](doc/issues/platform-validation.md).

- **Windows.** Group reordering uses actual foreground ownership and a single
  nonactivating native batch to avoid main flashing through pins or menus.
  Captionless resize and snapping use measured DWM contact bounds. Native child
  edges participate without adding layout margins. Cinema Viewer exit restores
  only visibility Unbound changed, preserves the selected floating viewer and
  yields to newer input, dialogs or application departure.
- **Cocoa.** Raise pin levels only for the active Resolve group and restore prior
  levels on departure, dialogs and teardown. Limit level tracking to members,
  the layout menu and actual popup/modal blockers. Temporarily parentless effect
  controls must never trigger a group scan on unrelated properties or lifecycle
  events. If host teardown clears Qt focus, synchronize only to the existing
  native key group window. Resolve owns integrated title movement, and the
  backend owns frame conversion, native resize gestures and measured clearance
  for traffic lights.
- **Inspection.** Follow the local `tools/gammaray/` README for native captures.
  Use a fresh session without Unbound and verify `truncated: false`. Live startup
  logs are `%LOCALAPPDATA%/Unbound/lua-bootstrap.log` and
  `~/Library/Logs/Unbound/lua-bootstrap.log`.
