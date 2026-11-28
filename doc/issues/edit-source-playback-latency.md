# Edit playback with populated Source

Open report: Unbound 0.7.1, a long MKV with proxies. Timeline playback becomes
laggy while the clip is loaded in Source; emptying Source restores smooth playback.
The reporter says uninstalling Unbound removes the issue. OS, Resolve version,
viewer mode, floating configuration and dropped frames versus transport delay
are not yet known. No cause or production fix is established.

## Code paths and scope

| Configuration | Viewer-specific behavior |
| --- | --- |
| Both docked, Single or Dual Viewer | Header buttons and observers remain. No viewer activation bridge, visibility override or space reclamation. Source visibility changes queue a coalesced reconciliation, not per-frame work. |
| Source floating, Program docked | Source gets a native host and activation bridge; its availability follows Resolve. No space reclamation. |
| Program floating, Source docked and visible | Program gets a native host and activation bridge; no space reclamation. A native attempt to hide Program is reversed asynchronously. |
| Program floating, Source hidden | Space reclamation expands the timeline. Showing Source disables reclamation and restores native allocation. |
| Both floating | Both bridges and native hosts; reclamation remains enabled. Source still follows native availability. |

`editviewers.h` does not query clip duration, format, proxy selection or playback
frames. Its Program visibility override is motivated by Single Viewer mode but
is gated by floating ownership/visibility, not an explicit Single/Dual mode flag.
`m_showingProgram` prevents immediate self-recursion; repeated native attempts to
hide Program could still produce repeated queued shows. This needs observation,
not an assumption that native playback does so.

`ViewerGeometryAdapter` holds root updates/main backing-store presentation during
retained-region geometry changes and releases them after correction. Floating
viewer updates remain enabled. Its application filter exists only during space
reclamation; ordinary stable playback updates are not suppressed. Repeated native
geometry/layout activity could nevertheless cause repeated correction work.

The activation bridge sends one page-context notification per genuine floating
host activation. It does not handle playback keys/frames or run with docked viewers.
Page focus, Cinema return and transition covers act on their own lifecycle events,
not merely because Source contains media. Window ordering is activation/visibility
driven; the broad macOS property/menu lifecycle scans are fixed in 0.7.1.

Shared controller discovery still observes widget construction and supported-slot
layout changes. If loading/playing this source repeatedly creates UI content,
measure those callbacks even in the fully docked case. A floating native renderer
also changes window/compositor context; preserving widgets does not prove zero
rendering overhead. Neither decoding contention nor a Qt composition problem is
established by this code review.

## Distinguishing comparisons

- Start with a fresh process, no desktop accessibility inspection, same timeline,
  playback range, window sizes and proxy preference. Separate fresh fully docked
  startup from docking after earlier floating use.
- Compare Source empty/populated with both viewers docked, then Source-only,
  Program-only and both floating in native Single/Dual modes where available.
  Record actual Source visibility and pointer position (Media Pool hover can
  change native preview visibility).
- Distinguish reduced playback FPS from slow transport response. Passively count
  Source/Program show/hide, activation, retained-region geometry, queued discovery
  and repaint holds. Stable counts with low FPS point away from repeated GUI work.
- If needed, compare original/proxy media and a shorter source while keeping the
  timeline fixed; do not assume which media Resolve actually decodes in Source.
- Compare installed/uninstalled fresh sessions under identical conditions before
  disabling individual Unbound mechanisms in a diagnostic build.

Existing viewer/Cinema fixtures pass visibility, activation, geometry correction,
paint-release and docking lifecycles. They do not model MKV decoding, proxy choice,
GPU scheduling or sustained native playback.

## Windows live comparison

The user's separate reproduction sequence is Dual Viewer, undock Program, then
switch to Single Viewer from its floating header. Source hides and timeline space
reclamation activates; Program playback reportedly becomes choppy. Local footage
is MOV, so this does not establish the same cause as the MKV/proxy report.

Initial playback briefly showed red 22.9 fps. After mode changes, sustained
playback showed green 25 fps and the full dock/undock/mode sequence did not retain
the slowdown. Passive eight-second measurements of Program's Qt `frameSwapped`
signal found approximately 25 presentations/second in both configurations:

| Configuration | Median interval | Longest interval |
| --- | --- | --- |
| Dual Viewer, Program floating | 40.1 ms | 43.2 ms |
| Single Viewer, timeline expanded | 40.0 ms | 42.4 ms |
| Repeated complete sequence to Single Viewer | 40.0 ms | 42.9 ms |

A passive main-window event trace showed viewer/timeline resizing at the mode
changes, not continually during playback. There is currently no evidence of a
reclamation loop. Presentation signals do not prove every image contains a fresh
decoded frame; screenshots cannot establish perceived motion smoothness. Desktop
capture is also a measurement confound. The transient remains unexplained: capture
it before further mode changes, which may clear its state. No fix is established.

The [native navigation investigation](native-edit-navigation-latency.md) concerns
cut-navigation stalls also observed without Unbound; it is not this report's cause.
