Fairlight audition: public Qt activation events only
===================================================

Purpose and status
------------------
Remove all Resolve/Qt-private memory inspection from the successful scope test,
and reduce the intervention to one notification per floating-host activation.
This is an isolated test, not yet a production change. Live validation passed on
the Xeon workstation with Resolve 21.1.1.10: user reports normal behavior in all
tested configurations. The event-only capture independently confirms native
player activation and preserved Qt focus/window identity (details below).

Build from the repository root:
  .\tests\platforms\windows\audition-events\build.cmd
Requires Windows x64, the normal toolchain and Qt 5.15.2 development package.
No private Qt headers/CorePrivate dependency is used. Output:
  build/windows/audition-events/Release/audition-events.exe
  build/windows/audition-events/Release/AuditionEvents.dll
Production sources, releases and artwork are unchanged.

Simple live pass, current normal .001 session
--------------------------------------------
No Resolve restart is necessary. Open Fairlight with an auditionable clip ready.
Run once from the repository root:
  .\tests\platforms\windows\audition-events\start.cmd

Use Media Pool normally: undock it and try audition Play; switch to the timeline
and back to Media Pool; dock it again. Stop then Play is always fine. Check audio,
waveform and Control Room meters. If those work, also try Space, returning from
another page, and a usual menu/dialog interaction. No exact order or timing and
no separate capture commands are needed. The first activation of the Media Pool
window after starting arms the normal event path; the helper does not force an
immediate activation simply because it was loaded.

Stop when done:
  .\tests\platforms\windows\audition-events\stop.cmd

Start automatically stops the previously loaded scope/context interventions, if
present, so they cannot make this comparison pass. It does not inject an older
helper just to stop it, and does not restart Resolve. Only one Resolve instance
may be open for this wrapper. The old helpers remain loaded but inactive.

Automatic records go to build/windows/audition-events/Release/logs/. The test
auto-stops after five minutes or 2500 records. Explicit Stop disconnects it and
flushes records. No immediate native context reset is forced at Stop; main-window
interaction provides the ordinary reset. --inventory is passive readiness only.
The injection launcher needs the normal same-privilege access to Resolve; the
helper itself never reads/writes Resolve private memory. No network is involved.

Mechanism
---------
The successful trace showed native scope changes on the top-level Media Pool
WindowActivate event BEFORE normal Qt FocusIn reaches its contents. The bridge
therefore handles that exact top-level event, after Resolve's application notify
handler has seen it. It sends one synthetic WindowActivate notification to the
existing UiMainWindow through QApplication::sendEvent. Resolve can select the
active page's scope normally; the genuine upcoming focus event can then activate
the audition player. There is NO synthetic FocusIn and no correction on clicks,
key presses, paint, resize, timer events or child activation propagation.

Only the exact synthetic event is consumed before ordinary QWidget delivery,
preventing fake main-window activation propagation to its children. The original
host activation proceeds normally. No activateWindow/setFocus or playback calls.
Stop during nested native processing still consumes the synthetic event before
removing the filter. Ordinary repeated activation of the SAME host is handled
each time, because native scope classification also runs each time; repeated
clicks/focus inside an already active pane do no corrective work.

Guards use only public Qt state: known Fairlight page, visible preview, its
current top-level window, exact known Unbound host name, current active window,
and absence of active popup/modal widgets. Docked content, other hosts, inactive
or minimized windows, hidden pages and propagated child events are excluded.
QPointer references tolerate destruction; preview->window() follows a newly
created host after redocking/undocking. No host-name search is needed on events.

AuditionActivationBridge itself needs only the already-known main/page/player
pointers, which a production pane session can supply. The standalone diagnostic
uses one PUBLIC QApplication::allWidgets inventory at Start to obtain them. It
does not repeat that inventory. Its logger listens to public Qt focus events and
player signals exposed through QMetaObject, and labels Play actions automatically.
It does not discover UiFocusedWidgetManager or record its state/signals.

No Resolve offsets, code fingerprints, private memory reads/writes, private
function calls, or Qt-private headers. The Qt 5.15.2 ABI requirement remains the
same as Unbound's startup-loaded helper. Widget names and native event behavior
are still application compatibility assumptions, so other Resolve versions need
ordinary validation even though there is no memory-layout dependency.

Validation performed
--------------------
- Actual Qt fixture verifies native application-notify ordering, exactly one
  main notification on host activation, genuine focus afterward, original input
  preserved, no notification on repeated clicks/focus or child activation, other
  host/page exclusions, safe stop during notify, docked behavior, and a new host
  after redocking. Fixture models context semantics and cannot verify real audio.
- Build/dependency audit passed. Generated helper sources/project have no private
  Qt dependency. DLL import audit found no ReadProcessMemory, WriteProcessMemory,
  VirtualProtect/Ex or QObjectPrivate imports. The standard injector necessarily
  transfers its DLL path to Resolve; that is distinct from reading private state.
- Passive live readiness check: 66453 widgets, two Fairlight players, zero manager
  discovery, no truncation, 66 ms total. No intervention enabled for that check.

Live result, 2026-10-13
-----------------------
User completed the ordinary usage pass and reported: "Acts normal in all configs."
Capture: build/windows/audition-events/Release/logs/
  focus-253048-20261013-154124-771.jsonl
Complete 88326 ms capture, explicit user-stop, zero off-thread signals skipped.
Eight player signal connections succeeded; no manager discovery or state reads.
Six floating-host activations each produced exactly one main notification. All
six preserved Qt focus and active-window identity across that notification.
Each was followed by native previewPlayer IsActiveChanged(true), 12-13 ms later,
with genuine focus in the floating player or Media Pool clip view. Docked and
floating audition Play, timeline handoff and docking back are recorded; no
additional corrections occurred on ordinary clicks or during docked playback.
Audio/visual quality is user-observed, not measured by these event records.

This establishes the event-only candidate on this Resolve/system combination.
Other versions and production startup/restoration integration still need normal
validation. The user's aggregate report is not a separately instrumented result
for every possible menu, modal, page or keyboard transition.
