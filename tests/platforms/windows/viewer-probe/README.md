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
size snapshot of currently floating Wheels, Curves, Warper and the native Timeline region, without changing
their state. Build with
`cmake --build build/windows/viewer-probe --config Release --target ColorLayoutSizeProbe palette-size-probe`.
Each invocation writes JSON under `%TEMP%/Unbound-palette-sizes/`, including widget
policies, minimums, hints, active stack pages, layout margins and spacing. No
recurring observer or private Resolve memory access is used. Loaded diagnostic
DLLs cannot be replaced until Resolve exits; they are not shipped.

`palette-trace-probe.exe` / `PaletteGeometryTrace.dll` record public Qt Resize,
Show and LayoutRequest events on Warper hosts, with immediate and queued height,
minimum and size-hint measurements. Build with
`cmake --build build/windows/viewer-probe --config Release --target PaletteGeometryTrace palette-trace-probe`.
Run once before a layout round trip. The observer removes itself after one minute
and writes at most 600 samples to `%TEMP%/Unbound-palette-layout.jsonl`. It never
consumes input or changes widget state. Wait for expiry before another capture.
