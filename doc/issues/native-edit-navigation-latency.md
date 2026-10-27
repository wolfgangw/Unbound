# Native Edit navigation latency after Source initialization

Investigate in Resolve without Unbound installed. The reported trigger is loading
a clip into Source in Dual Viewer mode: Up/Down cut navigation changes from
immediate to roughly half-second stalls on individual presses.

Public Qt/native-event observation confirms a GUI event-loop delay. After native
arrow-key receipt the first queued callback can run in about 1 ms, followed by
painting and a gap: a 100 ms single-shot checkpoint runs around 497–533 ms.
These are event-loop timings, not GPU presentation measurements. In a fresh
process the same probe shows the 100 ms checkpoint on time and later viewer
repaints at approximately 66–90 ms before Source is loaded.

Narrowing established so far:

- Reproduces with no Unbound runtime loaded; only the passive diagnostic is added.
- Explicit timeline focus does not remove it.
- Hiding Source via Single Viewer mode does not remove it.
- Moving Source away from its final frame does not remove it.
- Timeline Audio Scrubbing is already disabled.
- Clearing Source's recent clips empties its viewer but does not restore fast
  navigation; checkpoints remain delayed to approximately 489–516 ms.
- Timeline play/stop after clearing Source does not reset it.
- Restart removes the stall. Loading a timeline clip into Source, then its
  Media Pool version, then the original reproducing clip can all remain fast
  (later viewer repaint approximately 79–95 ms). Source loading alone is not
  a sufficient reproduction condition.
- User confirms navigation feels fast in the restarted state with Source loaded.
  Playing Source to its final frame does not recreate the half-second GUI stall;
  the 100 ms checkpoint remains on time (one later viewer repaint took 196 ms).

This suggests persistent native session/player state; the exact trigger is still
unknown. A synchronization wait is a hypothesis, not an identified cause.
No private Resolve code or object memory has been inspected and no production
workaround is justified yet. Do not add shortcut interception or change users'
playback/device preferences automatically.

Next comparisons: initial load order, loading Source at a saved final-frame
position in a fresh process, different media types, and distinguish decoding from
audio/video monitoring initialization using explicitly approved setting changes.
Keep timeline cuts and focus constant. Read-only logs may corroborate failures;
absence of logged errors does not rule out a native wait.

The bounded public-API timing diagnostic is documented in
[viewer-probe README](../../tests/platforms/windows/viewer-probe/README.md).
