Fairlight audition focus/context trace (Windows diagnostic only)
==============================================================

Purpose
-------
Compare Resolve's logical focus context and player signals in docked and floating
Media Pool. This observer is separate from the installed Unbound runtime.

Build from the repository root:
  .\tests\platforms\windows\audition-trace\build.cmd

Requires the normal Windows toolchain and Qt 5.15.2 MSVC x64 development package,
including its private headers. Output:
  build/windows/audition-trace/Release/audition-trace.exe
  build/windows/audition-trace/Release/AuditionTrace2.dll
The DLL name distinguishes it from an earlier, inactive inventory prototype
already loaded in the development Resolve session. No icons are regenerated.

Capture: docked and floating Media Pool
--------------------------------------
Install the candidate with DaVinci Resolve closed, then open Resolve normally,
load the project and open Fairlight. Unbound starts automatically.

1. Dock Media Pool using Unbound's Dock button. Select an auditionable clip and
   stop playback. Keep the same clip/timeline for both captures.
2. From a terminal at the repository root, run:
     .\tests\platforms\windows\audition-trace\start.cmd
3. Return to Resolve. Click the timeline and press Space to start it. Click the
   docked audition player's Play button: note audio and timeline handoff.
4. While audition is playing, click Undock. Note whether playback stops. Click
   audition Play in the floating pane; note audio, waveform and Control Room.
5. Click inside Media Pool and press Space twice, pausing between presses. Note
   which player responds. Then click the timeline and press Space to start it;
   click audition Play again to test the handoff from timeline to audition.
6. Dock Media Pool while audition is playing, then click audition Play again.
7. Run:
     .\tests\platforms\windows\audition-trace\stop.cmd
   Record the installed candidate and any unexpected observations.

Keep each capture under three minutes. No markers are required for the sequence
above; mouse, Space, focus and player events provide its ordering. An optional
mark.cmd appends a numbered snapshot and flushes records. Switching to a terminal
changes foreground focus; return to Resolve and click the relevant pane before
continuing. Those activation changes are also present in the capture.

Logs and controls
-----------------
Each start creates a timestamped focus-PID-UTC.jsonl under:
  build/windows/audition-trace/Release/logs/
Tell the investigator when each capture is complete; files are already local.
The launcher says "queued": execution happens on Resolve's GUI thread. Check
that a new file appears. Start while already running creates a checkpoint; stop
disconnects the observer. The DLL stays loaded until Resolve exits. Rebuilding a
loaded DLL requires a fresh process; replacing files cannot update loaded code.

The executable also accepts --start, --mark, --stop and --inventory directly.
Inventory makes a metadata/snapshot log and does not install ongoing observers.
UNBOUND_AUDITION_TRACE_DIR can override the log directory when set in Resolve's
environment before launch. No Unbound settings are read or written by the trace.

Captured evidence and limits
----------------------------
- UiFocusedWidgetManager::FocusChanged(int,int), with old/new context IDs.
- previewPlayer and widgetAudioPagePlayer: IsActiveChanged(bool),
  MuteAudioChanged(bool), FocusReceived(FrameRequestEventSource,bool), and
  TransportControlTriggered(TransportControlMode,bool).
- Custom-enum signals record occurrence only, not their argument payloads.
  Zero-argument slots avoid guessing private enum values/ABI. Default-argument
  FocusReceived clones are not connected as independent signals.
- Qt focus/activation, relevant Fairlight mouse/Space/parent/show/hide events,
  player window identity, existing native identity and page/slot ancestry.
  These are change notifications, not a query of initial player/context state.
  Establish context meanings by the docked timeline/audition actions in each run.

Discovery occurs once at explicit start: QApplication::allWidgets plus parent
and installed event-filter references find the otherwise parentless manager.
Qt 5.15.2 QObject private headers are used only to read event-filter references;
the runtime version is checked. No Resolve offsets, private function calls,
patches, playback commands, widget reparenting or event consumption are used.
No periodic scans, paint/resize logs or polling. Events buffer in memory and
flush at start, explicit mark, stop, application exit or automatic stop after
180 seconds / 2500 records. An abrupt process exit can lose unflushed events.
Signals emitted off the GUI thread are skipped and counted in the stop record.
The observer has finite overhead; note any new behavior while it is running.

Validation
----------
Build runs dependency audits and a Qt fixture verifying parentless event-filter
manager discovery, connections, context/bool payloads, custom-signal occurrence,
same-window/independent-window ancestry, stop/restart, ignored paint events and
the hard record cap. These fixtures cannot validate Resolve's audio behavior.

2026-10-13 live smoke check, Resolve 21.1.1.10, PID 159716: inventory found one
UiFocusedWidgetManager and both Fairlight players without truncation. All nine
signal connections succeeded. Observer explicitly stopped after about five
seconds. No playback or UI actions were automated. Paired user captures remain
necessary to identify whether focus context explains the audition failure.
