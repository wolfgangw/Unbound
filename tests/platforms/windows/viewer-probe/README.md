# Edit viewer investigation tools and integration tests

Viewer undocking now lives in `src/core/editviewers.h`; reclamation lives in
`src/core/viewergeometryadapter.h`. The normal controller installs both viewer
header buttons. There is no experiment control window or feature build switch.
Build an internal version with `./infrastructure/windows/build-release.cmd -InternalBuild`.
Program and Source use the normal settings instance, independent geometry and
floating intent. Source restoration waits for native availability; native
Single/Dual Viewer Mode remains controlled by Resolve.

The lifecycle fixture checks whole-widget reparenting, Source stack alternatives,
layout order/stretch, page hide/return, native geometry bursts, Inspector column
preservation, native divider restoration, close-to-dock and cleanup. It also checks
independent geometry recall after docking and after reloading the settings file,
startup floating restoration, and preserving floating intent during shutdown.
The main build runs it as UnboundViewerTests with a temporary settings file.

The separate inspection helpers can still be built from the repository root:

```powershell
cmake -S tests/platforms/windows/viewer-probe -B build/windows/viewer-dev -G "Visual Studio 17 2022" -A x64 "-DCMAKE_PREFIX_PATH=$env:USERPROFILE/Qt/5.15.2/msvc2019_64"
cmake --build build/windows/viewer-dev --config Release
```

`viewer-probe.exe` / `ViewerProbe.dll` queue one read-only targeted Qt widget and
layout snapshot per launch, written as viewers-PID-TIMESTAMP.json beside the DLL.
There is no recurring discovery. Captures may contain private project/clip names.
Open native menus before assessing their actions' enabled state in a snapshot.

`page-context-probe.exe` / `PageContextState.dll` capture one read-only inventory
of catalogued panes, page/auxiliary windows, ancestry, visibility and screen-mode
actions in `%TEMP%/Unbound-page-context/`. Use snapshots before/after native screen
mode changes to distinguish logical page departure from dock-slot reparenting.

`viewer-experiment.exe` / `ViewerExperiment.dll` are a standalone launcher for the
same integrated viewer code, without controls. They now use real settings. Do not
load this helper alongside Unbound, which already owns the viewer buttons.
Restart Resolve before testing rebuilt DLLs; replacing a file does not update
code already loaded in Resolve. Native Dual Screen transitions while viewers
float and more complex side-panel arrangements still need live validation.

The current production mechanism is documented in
`AGENTS.md` (Specialized pane mechanisms).

`main-resize-probe.exe` / `MainResizeGeometry.dll` temporarily attach the shared
main-window resize geometry observer to existing floating hosts. First launch
starts; second launch removes it and cancels its outstanding correction callbacks.
Use only on a build without the integrated observer. Recall a known layout before
starting, maximize/restore main through normal UI, then compare public host geometry
with the saved snapshot. It uses public Qt APIs, changes no native minimums, and
does not write layout snapshots. Stop it after the experiment.

`paint-trace-probe.exe` / `PaintTraceProbe.dll` passively record public Qt
geometry, layout, paint, main-window update and mouse-button event ordering.
First launch starts; a second launch stops and writes `paint-trace-TIME.json`
beside the DLL. It stops automatically after 60 seconds, recording at most
20,000 events in memory. Samples include widget names, geometry and update-enabled
states, with elapsed microseconds; no addresses, call stacks or private data are
inspected. The filter never consumes events or changes widget state. Use this
to distinguish child painting from top-level backing-store presentation.

`layout-state-probe.exe` / `LayoutStateProbe.dll` capture QObject identities and
Qt metadata for native layout controllers reachable through the application and
widget children. They do not inspect private connection structures, invoke
native slots, read custom property getters, or replace Qt's logging handler.
Parentless non-widget singletons may be absent. A capped capture reports
`truncated: true`; do not treat it as a complete QObject inventory.

Do not use QObject::dumpObjectInfo against Resolve for connection discovery.
Do not call that API in a live probe: its Qt invocation path is unsafe here.
Use passive signal/geometry observation; opaque native anchors are not a supported
position-setting interface.

`anchor-probe.exe` / `AnchorProbe.dll` observe the three Edit native layout
managers' `AnchorMoved(UiLayoutAnchor*,int)` signals, plus region geometry and
standard QWidget size limits. First launch writes an initial `anchors-*-start-*.json`
and starts observation; second launch disconnects its own subscriptions, writes
`anchors-*-stop-*.json`, and destroys the observer. It also stops after two minutes
and records at most 100 signal samples. Drag Resolve's native divider normally.
The observer never dereferences the opaque anchor pointer or calls native slots.
This is an inspection tool, not an anchor-position setter or reclamation feature.

`navigation-trace-probe.exe` / `NavigationLatencyProbe.dll` measure native Up/Down
key receipt, the next queued callback, a bounded set of paint/update events, and
single-shot event-loop checkpoints through one second. Build with
`cmake --build build/windows/viewer-probe --config Release --target NavigationLatencyProbe navigation-trace-probe`.
Run once to start and again to stop; observation expires after three minutes and
accepts at most 100 navigation commands. JSONL output is adjacent to the helper.
Only these two keys are recorded. The public Qt native-event filter never consumes
input or modifies focus, settings, widgets or player state. A late checkpoint
indicates GUI event-loop unavailability, not the identity of a blocking operation
or the time a GPU frame becomes visible. File logging adds measurement overhead;
compare conditions using the same probe. This diagnostic is not shipped.

`palette-size-probe.exe` / `ColorLayoutSizeProbe.dll` take a bounded public-Qt
size snapshot of all currently floating Unbound hosts and the native Color Timeline region, without changing
their state. Build with
`cmake --build build/windows/viewer-probe --config Release --target ColorLayoutSizeProbe palette-size-probe`.
Each invocation writes JSON under `%TEMP%/Unbound-palette-sizes/`, including widget
policies, minimums, hints, active stack pages, layout margins and spacing. No
recurring observer or private Resolve memory access is used. Loaded diagnostic
DLLs cannot be replaced until Resolve exits; they are not shipped.

`palette-trace-probe.exe` / `PaletteGeometryTrace.dll` record public Qt Resize,
Show, Hide and LayoutRequest events on Curves, Warper, Color Wheels and Color Match hosts, with immediate
and queued position, size, visibility, minimum and size-hint measurements. An
initial snapshot includes all floating hosts. Build with
`cmake --build build/windows/viewer-probe --config Release --target PaletteGeometryTrace palette-trace-probe`.
Run once before a page or layout round trip. The observer removes itself after
one minute, or on a second invocation, and writes at most 1,200 samples to
`%TEMP%/Unbound-palette-layout.jsonl`. It never consumes input or changes widget
state. The next capture replaces that file; copy it first if needed.

For failures that happen before a probe can attach, build with
`-InternalBuild -PaletteSizeDiagnostics`.
Each palette geometry-restoration attempt records its requested size, public Qt
host geometry/minimums, observer cancellation reason and queued event results to
`%TEMP%/Unbound-palette-restoration/`. This passive observer lasts ten seconds,
including after the bounded correction observer expires, and buffers at most
1,200 rows before writing a PID-qualified file. It does not resize, delay paints
or extend the correction lifetime. Start Resolve fresh and leave the affected
palette untouched for ten seconds to capture startup behavior.

## Native state inspection

`color-mode-probe.exe` / `ColorModeState.dll` take one read-only public-Qt
snapshot of Color buttons, player/Lightbox widgets and native Lightbox/transport
menu actions. Build targets `ColorModeState` and `color-mode-probe`. Launch hidden
and wait as below; output goes to `%TEMP%/Unbound-color-mode/`. Open the native
menu before relying on its action state: Resolve refreshes it on opening.
The capture reads standard QWidget/QAbstractButton/QAction properties and method
metadata only; it never invokes Resolve slots or reads custom property getters.
There is no recurring observation. Compare transport state and native mode
presentation separately from the toolbar's checked state.

`color-transport-probe.exe` / `ColorTransportState.dll` take a one-shot read-only
snapshot of standard Color viewer labels, buttons and sliders, including the
explicitly hidden state of `labelPlaySpeed`. Build targets `ColorTransportState`
and `color-transport-probe`; output goes to `%TEMP%/Unbound-color-transport/`.
Custom properties/methods are listed as metadata only, never invoked. Hidden
speed text can retain an old shuttle value; compare explicit visibility as well
as text. Launch hidden and wait; do not rebuild a probe already loaded in Resolve.

`lightbox-trace-probe.exe` / `LightboxTrace.dll` observe existing public
`btnColorToggle*` buttons: clicked/toggled signals, Show/Hide/EnabledChange events,
checked/enabled/visible state and ancestor visibility. First launch starts, second
stops; a two-minute deadline and 2000-snapshot limit bound the trace. Queued and
100 ms post-click samples distinguish click delivery from settled native state.
Output goes to `%TEMP%/Unbound-lightbox-traces/`. Build targets `LightboxTrace` and
`lightbox-trace-probe`; launch hidden and wait as for the probes below. It does not
click controls, change playback, call native slots or read private memory.

`dual-screen-probe.exe` / `DualScreenState.dll` take one read-only GUI-thread
snapshot per launch using public Qt widget metadata. Build targets `DualScreenState`
and `dual-screen-probe`, then launch the executable hidden and wait. Timestamped
JSON goes to `%TEMP%/Unbound-dual-screen/`. Compare object identities, parents,
top-level windows, visibility and geometry across native mode toggles. The tool
does not change the mode or install an observer. Its 30,000-widget cap can truncate
large sessions: check `truncated` and never infer absence from an incomplete dump.
Replacing a loaded diagnostic DLL requires a fresh Resolve process.

`toggle-state-probe.exe` / `ToggleState.dll` snapshot catalogued pane and toggle
ancestry, window identity, visibility and button state through public Qt APIs.
Build targets `ToggleState` and `toggle-state-probe`; launch hidden and wait.
Each launch replaces `%TEMP%/Unbound-toggle-state.json`. No UI state is changed.


`event-gap-probe.exe` / `EventGapTrace.dll` observe gaps between GUI-thread Qt
notifications, bounded to 60 seconds. Launch once to start and again to stop;
output is `%TEMP%/Unbound-event-gaps.json`. A gap after an event does not prove
that event consumed the interval: native dispatch and nested work can intervene.

`gui-stack-sample.exe <Resolve PID> <output.txt>` is a standalone Windows diagnostic,
linked with DbgHelp, never shipped or loaded into Resolve. It samples the main
window thread for 30 seconds at approximately 25 ms intervals, briefly suspending
it to unwind each stack. Symbol names are resolved after sampling. Export-only
names can be far from the actual function; inspect the decimal displacement and
do not interpret distant names as function identities. Sampling perturbs timings.
Use stack samples only to identify likely cost centers, not to measure elapsed time. Desktop automation/accessibility clients can add substantial native
accessibility work; compare manual page switches without concurrent UI inspection.

`host-constraints-probe.exe` / `HostConstraints.dll` compare floating hosts'
QWidget minimums, QWindow minimums and Windows `WM_GETMINMAXINFO` track limits.
Build both targets and launch hidden; output is
`%TEMP%/Unbound-host-constraints.json`. This is a one-shot GUI-thread observation;
it does not resize windows or install hooks. Native track sizes include frame
extents and must not be compared directly with Qt client sizes.

`main-placement-probe.exe` / `MainPlacementTrace.dll` observe Qt geometry/state
events and native window-position messages before and after Resolve's handler.
Build both targets, launch hidden to start and again to stop; a 30-second deadline
and 5000-row limit bound collection. Output is
`%TEMP%/Unbound-main-placement-trace.json`. Compare the proposed native rectangle,
client minimum and frame margins across maximize/restore; observation does not
change placement or message parameters.

`scopes-state-probe.exe` / `ScopesState.dll` take a one-shot public-Qt snapshot of
Scopes widgets, ancestry, geometry, widget/override cursor state and meta-object information in
`%TEMP%/Unbound-scopes-state.json`. Build both targets and launch hidden. The
read-only snapshot does not establish whether the rendered signal is current;
verify that visually with native mode/frame changes.

Reversible research prototypes mutate live UI state and must not run alongside
the corresponding integrated feature. Build their paired targets, launch hidden
once to enable and again to remove. Replacing a loaded DLL requires a fresh process.

- `scopes-embed-probe.exe` / `ScopesEmbed.dll` embeds an already-open native
  `UiWaveformWindow` in a temporary host. Removal returns its original parent,
  flags, geometry and minimum and shows the native window again. Use native Scopes
  controls to test signal updates; the prototype is not a production docking path.
- `main-placement-restore-probe.exe` / `MainPlacementRestore.dll` attaches the shared
  main-placement controller with isolated settings at
  `%TEMP%/Unbound-main-placement-experiment-<PID>.json`. Removal detaches it;
  production settings are not rewritten by this diagnostic controller.
- `main-resize-probe.exe` / `MainResizeGeometry.dll` attaches shared geometry
  preservation to currently floating hosts. Removal detaches its observers;
  compare host geometry across native main-window maximize/restore or resizing.
- `scopes-layout-refresh-probe.exe` / `ScopesLayoutRefresh.dll` attaches the shared
  Scopes view-count refresh to an already embedded native dialog. Launch again
  to remove it; it also expires after five minutes. Test resizing and layout recall
  with the menu open, checking native button availability and exact host geometry.
