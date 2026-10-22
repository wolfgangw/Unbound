# Edit Media Pool collapsed width

Status: the same collapsed allocation is reproducible in Resolve without Unbound.
Native recovery is established. Unbound now checks docking allocation and returns
collapsed panels to floating state. The user confirmed the recovery and warning UI
in a fresh Resolve session using the project-resolution reproduction below.

## Report and confirmed state

An Edit Media Pool that had been floating disappeared after an unidentified
click. The session had a floating Program Viewer in Single Viewer Mode, with
Unbound reclaiming the viewer region for the timeline. Inspector and Mixer also
remained floating.

The installed helper's SHA-256 matched the published Windows 0.3.0 build. The
subsequent page-focus changes were not loaded in this session.

Before recovery, saved settings recorded Media Pool as docked and its native
visibility toggle as checked. A read-only snapshot on Resolve's Qt GUI thread
confirmed:

- No floating Edit Media Pool host remained.
- `frameEditPageMediaPool` and its native content were parented under main,
  visible and not explicitly hidden. The slot geometry was `[0,0,1,387]`:
  only one pixel wide, with no minimum width.
- The native Media Pool toggle was checked.
- `frameEditPageVideoPlayers` was `[0,0,1713,0]` and
  `frameEditTimelinesHolder` was `[0,0,1713,661]`. The reclaimed timeline
  occupied the full region width.
- The user reported that toggling Media Pool had not recovered it; inspection
  after automated toggle attempts also found the same collapsed state.

This occurrence is a docked-slot geometry failure, not an offscreen or minimized
floating host. The user subsequently reproduced the disappearance by clicking
Unbound's docking button again, with the main window near its minimum height.
The original unidentified click is no longer needed to reproduce the symptom in
that session.

Passive GUI-thread event and signal tracing confirmed that an ordinary click on
the native Media Pool toggle unchecks it and then Resolve's own click handler
immediately checks it again. The panel remains collapsed. A captured call stack
placed the recheck in Resolve's native layout handler, not Unbound's restoration
code. Comparison with the vertical-video preference disabled establishes that
the forced selection belongs to the optimized vertical layout.

Once present, the failed allocation survived docking the Program Viewer,
maximizing the main window and leaving/returning to Edit. Repeated Media Pool
undock/dock also reproduced it in that already-affected state. These comparisons
do not establish whether low main-window height or viewer reclamation initially
caused it. Neither comparison alone isolates the original trigger.

## Vertical-video layout comparison

The affected project uses a portrait timeline and Preferences > User > UI
Settings > **Use optimized UI layouts for vertical video** is enabled.

From a fresh session, docking Media Pool succeeded, including with Program
Viewer floating and the main window at its minimum height. The native sidebar
still occupied space while Media Pool floated: the native toggle handler would
not permit the last selected left-hand panel to be switched off.

With Program Viewer docked and the main window maximized:

1. Disabling vertical-video optimization restored ordinary Media Pool toggle
   behavior. Media Pool could be hidden through its native button.
2. Enabling vertical-video optimization with Media Pool hidden reproduced the
   visible one-pixel slot and checked Media Pool button. The viewer occupied
   the full width even though its geometry adapter was inactive.
3. Disabling optimization again recovered the panel. Re-enabling it with Media
   Pool visible preserved a usable native column. Subsequent Media Pool
   undock/dock worked, including after recalling the small-window arrangement
   with a floating Program Viewer.

These observations show that minimum height and an active viewer adapter are
not necessary for this reproduction.

### Confirmed without Unbound

The same sequence was repeated in a fresh Resolve Studio 21.1.1.10 process with
Unbound's startup hook removed from the Scripts tree. Module inspection confirmed
that neither Unbound nor diagnostic probes were loaded during the reproduction.
Only ordinary Resolve controls were used:

1. Open the portrait-timeline project on Edit and maximize the main window.
2. Disable **Use optimized UI layouts for vertical video** and save preferences.
3. Hide Media Pool using the native toolbar button.
4. Enable vertical-video optimization again and save preferences.
5. Click Media Pool. Its button is checked, but the panel remains inaccessible.

A read-only GUI-thread snapshot loaded **after** the visible failure confirmed
`frameEditPageMediaPool` visible at `[0,0,1,790]`, its native toggle checked, and
the viewer region occupying `[0,0,3440,790]`. This reproduces the distinguishing
geometry of the original failure without Unbound's reparenting or adapter.
It establishes a native Resolve layout defect, but does not prove which action
originally put the user's floating-panel session into that state.

Recovery: disable vertical-video optimization, ensure Media Pool is visible,
then re-enable optimization. This restores the original preference while
rebuilding a usable native column. No workspace reset is required.

### Project-resolution transition with the preference unchanged

A fresh empty project provides a second reproduction without changing the user
preference. With optimization enabled throughout and all panes docked:

1. Set the project timeline resolution to 1920x1080.
2. Hide Media Pool through its native toolbar button.
3. Change the project resolution to 1080x1920 and save.
4. Click Media Pool. The native button is checked, but its slot is one pixel
   wide and the viewer occupies the entire available upper region.

Entering portrait mode with Media Pool already visible instead preserves a
usable column. This comparison was performed with Unbound loaded; the separate
preference-based reproduction above establishes the native-only failure.

The same resolution transition reproduces the floating-panel disappearance:
undock Media Pool in landscape mode, which normally turns its native toggle off;
change the project to portrait; then dock Media Pool. Before docking, a read-only
snapshot already shows the native slot at `[0,0,1,783]`, the native toggle checked,
and the viewer at `[0,0,3019,783]`. The floating content is still usable. Docking
returns that content to the one-pixel slot and closes its floating host, making it
inaccessible. The geometry remains collapsed after docking. Program Viewer was
docked and the main window maximized throughout the resolution change and docking.

This identifies a complete path to the symptom: entering the optimized layout
with the native Media Pool hidden can leave its column collapsed; subsequent
docking exposes the failed allocation. It does not establish the original
unidentified click or the precise error inside Resolve's private layout solver.

Healthy portrait-mode comparisons did not reproduce the collapse through simple
Media Pool undock/dock, a floating Program Viewer combined with minimum main-window
height, native Expand View, or dragging the native column divider. With Effects
visible, Media Pool could be switched off; Resolve then refused to switch off
Effects as the last selected left panel. The forced selection therefore applies
to the last left panel, rather than Media Pool unconditionally.

Returning the test project to landscape recovered the checked Media Pool;
returning to portrait with it visible retained the healthy column. This is a
diagnostic recovery, not a proposal for Unbound to modify project resolution.

## Recovery and investigation boundary

At the user's request, a one-shot GUI-thread recovery resized only the docked
Media Pool slot to 600 pixels wide and raised it above sibling widgets. Its
content and Unbound header button became visible again. The timeline retained
its full-width rectangle, so the recovered panel overlaid part of the timeline.
This makes the controls reachable but does not repair native column allocation.
No production helper code or saved layout snapshot was changed by the recovery.

Making the existing native sidebar divider reachable and dragging it could
change Media Pool's width, but did not move the viewer's left edge to reserve a
column for it. A subsequent native toggle recalculated the collapsed allocation.
Widget width alone is therefore insufficient to repair the underlying layout.

The diagnostic used standard QWidget geometry/visibility and layout metadata;
it did not call private Resolve handlers or modify native anchor fields.
Snapshots and the pre-recovery settings copy remain in ignored build output.

The adapter limits timeline width using the native viewer rectangle;
the captured full-width viewer baseline left it no Media Pool column to reserve.
That explains the overlap but does not establish why the native column became
one pixel wide. Do not turn the one-time 600-pixel recovery into a fixed-width
production workaround. Unbound's shared docking safeguard checks the allocated
area after layout settles and automatically floats the content again if it stays
collapsed, preserving geometry and saving the actual floating state. Its button
pulses until the user opens that panel's context menu; the explanation suggests
disabling vertical-video optimization for Edit Media Pool. This contains the
failure without modifying Resolve's preference or repairing its private layout.
Fixtures cover collapse, ancestor clipping, temporary sizes, page deferral,
warning acknowledgment and persistence. The user also validated the Media Pool
recovery and warning UI in Resolve; other native failure scenarios remain covered
by fixtures rather than confirmed live reproductions.
