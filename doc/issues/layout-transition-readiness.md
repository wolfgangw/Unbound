# Layout transitions: open work

## Mechanism and limits

[AGENTS.md](../../AGENTS.md#layout-transition-presentation) owns the shared lifecycle,
readiness and cancellation contract. Windows hides intermediate teardown/build-up
with a desktop cover and fade; macOS retains native presentation without capture
or permission requests. Visual continuity takes priority over speed.

Readiness is bounded evidence from queued work, stable geometry, public GL composition
and raster acknowledgements, not proof that all native rendering has finished.
A deadline is recovery, not readiness. All Qt work stays on the GUI thread; do not
retain empty hosts/compositors or alter context sharing to accelerate transitions.

## Windows 10 capture limitation

On the tested Windows 10 build 19045 workstation, capture exclusion of the translucent
layout menu fails with `ERROR_NOT_ENOUGH_MEMORY` (8), before screenshot allocation.
The failure is specific to the tested translucent-window path; actual memory exhaustion
or a universal Windows 10 limitation is not established. Windows 11 is validated.

Production skips the cover and continues recall. This workstation limitation is
accepted: retain the current rendering path, keep/report the failing fixture, and
do not introduce an opaque-menu workaround. Other Windows 10 systems need validation.

## Remaining validation

- Page/layout switches with live playback, floating viewers/audition, unavailable
  Source and first visits; preserve focus, transport and timecode.
- Outgoing capture (including a lone pane), docked destinations, multiple displays/DPI,
  rapid recalls, dialogs, pinned peers and interactive menus at screen edges.
- Color Lightbox/Clips mode changes with docked/floating Viewer and forward/reverse/
  shuttle playback; verify looping, cancellation and final docking allocation.
- Layout dirtiness against actual saved geometry, native minimums and main placement;
  include user-resize cancellation and changed DPI.
- Delayed native/GPU work, occlusion and peak transient memory including compositor
  costs; bitmap size alone is not total RAM.

## Deferred

Color All Out ? fully docked Edit can remain slow despite no pending pane/toggle
work. Native resize, accessibility and GL costs are candidates; no low-impact fix
is established. Preserve native accessibility and queued placement.

Live viewer/scope patches, further performance optimization and a small delayed
progress indicator are deferred. No indicator design is selected.
