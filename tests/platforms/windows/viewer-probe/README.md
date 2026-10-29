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

`viewer-experiment.exe` / `ViewerExperiment.dll` are a standalone launcher for the
same integrated viewer code, without controls. They now use real settings. Do not
load this helper alongside Unbound, which already owns the viewer buttons.
Restart Resolve before testing rebuilt DLLs; replacing a file does not update
code already loaded in Resolve. Native Dual Screen transitions while viewers
float and more complex side-panel arrangements still need live validation.

The current production mechanism is documented in
`AGENTS.md` (Specialized pane mechanisms).

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

`layout-trace-probe.exe` / `LayoutTraceProbe.dll` passively observe parent,
visibility and geometry events on the Edit main/auxiliary containers, viewer
region, timeline holder and relevant toolbar stack. First launch starts tracing;
a second launch stops, removes filters and writes `layout-trace-PID-TIME.json`.
Tracing also stops after two minutes, with at most 500 recorded events. Call
stacks are captured only for these events; there is no polling or interception
of native handlers. Use native menus normally while tracing. Raw addresses must
be normalized using the capture's Resolve image base and the exact binary before
on-disk analysis. They are diagnostic evidence, never callable Unbound APIs.

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
Show, Hide and LayoutRequest events on Curves and Warper hosts, with immediate
and queued position, size, visibility, minimum and size-hint measurements. An
initial snapshot includes all floating hosts. Build with
`cmake --build build/windows/viewer-probe --config Release --target PaletteGeometryTrace palette-trace-probe`.
Run once before a page or layout round trip. The observer removes itself after
one minute, or on a second invocation, and writes at most 1,200 samples to
`%TEMP%/Unbound-palette-layout.jsonl`. It never consumes input or changes widget
state. The next capture replaces that file; copy it first if needed.

For failures that happen before a probe can attach, build with
`-InternalBuild -PaletteSizeDiagnostics` (optionally alongside `-LayoutCoverExperiment`).
Each palette geometry-restoration attempt records its requested size, public Qt
host geometry/minimums, observer cancellation reason and queued event results to
`%TEMP%/Unbound-palette-restoration/`. This passive observer lasts ten seconds,
including after the bounded correction observer expires, and buffers at most
1,200 rows before writing a PID-qualified file. It does not resize, delay paints
or extend the correction lifetime. Start Resolve fresh and leave the affected
palette untouched for ten seconds to capture startup behavior.

## Integrated layout recall trace

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

`cover-event-probe.exe` / `CoverEventTrace.dll` observe QDialog Show events and Qt
application-state changes, recording object names/classes, window/modal flags and
the actual Windows foreground process relationship. First launch starts, second
stops; it expires after 60 seconds with at most 1000 rows. It writes
`%TEMP%/Unbound-cover-events.json` without titles, pixels or input changes. Build
the `CoverEventTrace` and `cover-event-probe` targets; use the shared launcher with
`Start-Process -WindowStyle Hidden -Wait`. Do not rebuild a loaded probe.

`cover-memory-probe.exe` / `CoverMemoryProbe.dll` run three bounded capture/hold/
release cycles using the experiment's native cover backend, without recalling a
layout. Keep Resolve foreground, idle and playback stopped. Each cover lasts
300 ms followed by 700 ms for cleanup. Samples before capture, after capture,
while held, after release and after cleanup record process private/resident bytes
and public DXGI usage, under `%TEMP%/Unbound-cover-memory.json`. Build the
`CoverMemoryProbe` and `cover-memory-probe` targets; launch hidden and wait as above.
Interruption ends the experiment. Raw image storage, private commit and resident
memory overlap and must not be added. DXGI counters belong to Resolve and exclude
separate compositor-process allocations; sampled peaks are not exhaustive maxima.

`shell-memory-probe.exe` / `ShellMemoryProbe.dll` sample process private commit,
resident working set and public DXGI per-process local/non-local GPU usage six
times over eight seconds. By default they only observe. To measure cache release,
write `%TEMP%/Unbound-shell-memory-request.json` with the current Resolve `pid` and
`"release": true`, then launch the probe. It consumes that request and, at 2.5 s,
deletes only hidden Unbound hosts with no layout and no QWidget descendants.
An optional `"hosts"` array restricts release to those exact observed object names;
an empty array releases nothing. Omit the array to release every eligible shell.
Their QPointer owners recreate them on the next undock. Visible hosts and native
content are never released. Output is under `%TEMP%/Unbound-shell-memory/`.
Keep playback stopped and the UI idle; allow native work and earlier traces to
finish before measuring. Compare stable before/after samples in the same process.
Private commit and resident working set overlap; never add them. DXGI may expose
the same hardware adapter more than once; do not sum duplicate adapter entries.
Driver/allocator caches and unrelated Resolve cleanup can delay released bytes.

Build with `cmake --build build/windows/viewer-probe --config Release --target ShellMemoryProbe shell-memory-probe`.
`ShellTrimProbe` / `shell-trim-probe` use the same protocol but shrink those empty
hosts and their public QBackingStore allocation to 1 by 1 instead of deleting
them. This is a diagnostic experiment, not a production memory policy. Both use
the same observation guard; do not overlap them. Do not rebuild a loaded probe.

`composition-trace-probe.exe` / `CompositionTraceProbe.dll` pair the public
QOpenGLWidget `aboutToCompose`/`frameSwapped` and framebuffer resize signals for
surfaces present at attachment. First launch starts, second stops; a 60-second
deadline also stops it. It buffers at most 12,000 rows and writes once to
`%TEMP%/Unbound-composition-traces/`. No rendering settings, input or widgets are
changed. Build with `cmake --build build/windows/viewer-probe --config Release --target CompositionTraceProbe composition-trace-probe`.
Use `Start-Process -WindowStyle Hidden -Wait` for the launcher, particularly when
combining probes: the shared launcher guard rejects overlapping launches.
Several children in one window report the same composition; do not add those
durations together. First composition includes compositor setup, not just swap.

Build an internal installer with
`infrastructure/windows/build-release.cmd -InternalBuild -LayoutTrace`.
The shared CMake option is `UNBOUND_LAYOUT_TRACE=ON`; it is OFF by default.
Install only with Resolve closed and validate in a fresh Resolve process.

Every explicit layout recall starts a six-second, GUI-thread observation window.
With `-LayoutCoverExperiment`, page switches to floating content also start
one. `page_capture_arrival` marks capture at incoming Show after identifying the
destination's floating content/intent. `page_departure_to_capture` measures the
gap from outgoing Hide; `page_precapture_paint_deliveries` and
`page_precapture_update_requests` count
main-window activity in it, without asserting GPU presentation.
`page_incoming_show` and `page_current_changed` identify the handoff. The trace's
page field identifies the destination. Any fully docked destination generates
neither captures nor new traces, regardless of outgoing floating panes. Compare buttons
and native shortcuts, first visits, returning hosts and rapid switching; use the
same final work/geometry/composition and cancellation checks as layout recall.
Another recall ends the old trace as superseded. Output is buffered in memory
(up to 6000 milestone rows and 1024 paint/swap summaries) and written once afterward to
`%TEMP%/Unbound-layout-traces/*.json`. Logs contain object identities, timing and
counts, not project names, layout names, pixels or key contents. There is no idle
polling or paint suppression. Native public GL frame signals are observed when
available; zero GL samples do not imply playback stopped. Qt frameSwapped signals
describe top-level composition, not whether that individual widget's content
changed. Paint filter samples are event delivery, not completed rendering.
Instrumentation adds overhead.

Structural events and event-loop idle notifications use numeric aggregates, with
100 ms buckets over the observation window; they cannot crowd out milestone rows.
Phase aggregates and terminal status survive even if the milestone limit is hit.
Public GL surfaces have stable trace-local IDs across reparenting, with initial
class/window/ancestry metadata. Gap summaries include the gap end time and first
sample time. `UNBOUND_LAYOUT_TRACE_DIR` overrides output for isolated fixtures.

Controller spans measure recall/discovery/undock/dock; settings spans include
serialization and atomic commit. Pending-state snapshots and a provisional 50 ms
structural quiet marker are recorded alongside late docking/palette corrections.
A quiet marker is NOT a GPU-ready signal. The trace does not delay the layout.

Schema 4 records pending discovery, palette geometry correction,
docking checks, Clips initialization/allocation, Edit reclamation and main
placement through diagnostic work markers. Work queued before recall is included;
completion, cancellation and owner destruction clear it. An idle palette observer
does not block solely because its bounded lifetime has not expired. The
composition snapshot compares each visible window's latest geometry revision
with a public GL composition beginning and ending in that same revision/window.
Raster windows remain unproven in trace-only builds. These markers remain
experimental. Header refresh and branch indexing spans help
separate repeated discovery work from native layout/composition costs.

Build the optional Windows cover with
`infrastructure/windows/build-release.cmd -InternalBuild -LayoutCoverExperiment`.
This implies tracing and is rejected for non-internal builds. Add
`-PaletteSizeDiagnostics` for startup palette evidence. Install with Resolve closed
and restart. Presentation behavior and limits are defined in
[AGENTS.md](../../../../AGENTS.md#layout-transition-presentation).

Inspect `cover_end`: `revealed` differs from `timeout_not_ready`, capture failure
and cancellation. `cover_bitmap_bytes` measures raw temporary images only.
`post_update_candidate_not_native_ready` is provisional presentation evidence.

Summarize files with `python tests/platforms/windows/viewer-probe/summarize-layout-trace.py <file.json> ...`.
Span durations are inclusive/nested and must not be summed into a total.
Remaining native validation is tracked in
[layout-transition-readiness](../../../../doc/issues/layout-transition-readiness.md).

`menu-transition-probe.exe` / `MenuTransitionTrace.dll` observe menu visibility,
geometry, activation and native window-order events without changing them. First
launch starts, second stops; a two-minute deadline and 5000-row cap bound the
observer. Output goes to `%TEMP%/Unbound-menu-transition.json`. Each event records
whether Windows still has this process foreground and whether the menu or the
cover owns hit testing at its center. Build targets `MenuTransitionTrace` and
`menu-transition-probe`; launch hidden and wait. Native ordering is diagnostic
only; the observer never changes focus, visibility, capture affinity or input.
