# Layout transitions: remaining validation

The implemented presentation, restoration and cancellation contracts are in
[AGENTS.md](../../AGENTS.md#layout-transition-presentation). The Windows cover is
included in release and internal builds; macOS retains ordinary presentation without capture
or permission requests. Visual continuity takes priority over latency.

## Accepted Windows 10 translucent-menu capture failure

On the Windows 10 workstation, the native layout-cover fixture fails when
`SetWindowDisplayAffinity(WDA_EXCLUDEFROMCAPTURE)` is applied to the production
layout menu. The call returns false with error 8 (`ERROR_NOT_ENOUGH_MEMORY`),
before screenshot allocation; application foreground ownership is still correct.
The error alone does not establish actual memory exhaustion.

Controlled comparisons isolate Qt's per-pixel translucent window path:
the ordinary popup passes, enabling `WA_TranslucentBackground` on that popup
produces the same failure, and disabling it before showing the production menu
makes the complete fixture pass. This is not an activation-bridge failure or
an intermittent assertion. The user confirms working layout transitions on the
other Windows 11 system. Behavior on other Windows 10 systems remains unverified;
this failure does not establish a universal Windows 10 limitation.

Production currently cancels presentation as `capture_unavailable` and continues
layout recall without its cover, exposing intermediate rearrangement. The user
accepts this workstation's failing test for now: retain the existing rendering
and transition mechanism, including the working Windows 11 path. Do not introduce
an opaque-menu workaround or another transition mechanism. Keep the test and
report its failure explicitly rather than treating validation as fully passing.
Microsoft documents capture exclusion support from Windows 10 version 2004;
this workstation's build 19045 meets that OS requirement, but successful operation
on another qualifying Windows 10 system remains a possibility, not a verified result.

## Validation gaps

- Floating-to-floating page switches, active playback, floating Edit viewers,
  unavailable Source, Fairlight audition players and first visits to unsupported
  pages. Check focus, transport routing and timecode continuity.
- Verify departure capture preserves outgoing hosts, including a lone Media Pool,
  and promptly releases on fully docked destinations without a restoration delay.
  Departure/arrival capture must conceal host removal and incoming restoration
  across displays and DPI changes. Qt event ordering cannot prove that native/GPU surfaces have not presented.
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

- Verify Timeline docking allocation and docked Viewer sizing across Color layout
  recalls, including active playback and native minimum-size changes. macOS
  layout recall → Dock all → recall is validated; broader combinations remain open.
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

A subtle delayed progress indication remains deferred. A longer covered transition
can look inactive before the final fade. The indicator should stay visually small,
avoid flashing during fast transitions, and remain responsive while the GUI thread
is occupied. No visual design or implementation is selected.
