# Layout transitions: remaining validation

The implemented presentation, restoration and cancellation contracts are in
[AGENTS.md](../../AGENTS.md#layout-transition-presentation). The Windows cover is
an opt-in internal feature; macOS retains ordinary presentation without capture
or permission requests. Visual continuity takes priority over latency.

## Validation gaps

- Floating-to-floating page switches, active playback, floating Edit viewers,
  unavailable Source, Fairlight audition players and first visits to unsupported
  pages. Check focus, transport routing and timecode continuity.
- Verify departure capture preserves outgoing hosts, including a lone Media Pool,
  and promptly releases on fully docked destinations without a restoration delay.
  Departure/arrival capture must conceal host removal and incoming restoration
  across displays and DPI changes. Zero Qt paint events before capture do not
  prove that native/GPU surfaces have not presented.
- Rapid recalls, dialogs, application departure, pinned peers and menus near
  display edges. Verify actual menu input and capture exclusion, including the
  translucent menu on supported Windows versions.
- Color Lightbox entry/return and retained-Clips recall during forward, reverse
  and shuttle playback, with the viewer docked and floating. Verify native mode,
  looping, resume cancellation and final presentation, not just toolbar state.
- Readiness coverage for delayed native work, occlusion and non-Qt surfaces.
  Deadline expiry is failure recovery, never evidence of readiness.
- Peak transient memory, including native/compositor allocations. Raw bitmap
  storage is not total RAM; private commit, resident RAM and GPU usage overlap.

## Open restoration cases

- Color All Out to All Docked can make Timeline's allocation safeguard reject the
  first docking attempt; repeating recall succeeds. Diagnose allocation/toggle
  ordering without weakening the safeguard or silently resaving layouts.
- A Color All Out to Grid recall after a rejected playing transition can leave
  the docked Viewer compressed below unused allocation. Isolate native mode and
  retained allocation from playback assistance.
- Check unexplained layout dirtiness after page round trips against actual saved
  geometry; respect persistent native minimums and immutable layout snapshots.
  Broaden main-placement settlement coverage to user resize cancellation and
  changed DPI.

## Page-switch latency

Color All Out to fully docked Edit remains slower than expected, including without
desktop inspection. Long gaps occur with no pending pane restoration or toggle
requests. Main-window size changes, native accessibility processing and GL
presentation are candidate costs; their relative contributions remain unproven.
No low-impact fix is established. Preserve native accessibility and queued main
placement; queued-work markers do not measure callback CPU time.

## Deferred work

Live viewer/scope patches and further performance optimization are deferred.
Qt already coalesces updates and double-buffers widgets; it provides no public
atomic presentation transaction across independent QWidget/GL windows. All widget
work stays on the GUI thread. Do not change Resolve's context-sharing policy,
force repaint attributes or retain empty hosts to reduce transition time.

Readiness remains a bounded heuristic: completed Unbound work, stable geometry,
revision-matched public GL composition and queued raster update acknowledgements
cannot prove that Resolve has no later native rendering work. A hidden marker,
zero timer, event-loop idle signal or fixed quiet period cannot supply that proof.

Diagnostic build commands, outputs and interpretation are in the
[viewer-probe README](../../tests/platforms/windows/viewer-probe/README.md).
