# Color clip/node selection latency

## Report

Unbound 0.6.2 with macOS Resolve 21.2: selecting clips or nodes on Color can cause
substantial delay and the application busy cursor, including with everything
docked. Uninstalling removes the symptom; reinstalling restores it. Whether this
also happens before any undocking in a fresh process is unknown.

## Code findings

Selecting a clip changes the active grade/node graph and tool values. Selecting a
node changes the tool values and potentially their native UI. Unbound does not
subscribe directly to either selection; native widget creation, visibility and
layout events can nevertheless trigger its discovery/decorating work.

The following paths exist in v0.6.2:

- Controller application filtering queues discovery for child creation under any
  QWidget, and for layout requests on catalogued panes. Deferred work coalesces
  within one event-loop turn, not across a multi-turn native UI update. Even a
  child that is not a widget can cause a discovery pass after being discarded.
- Every discovery refreshes all registered Color control headers, including
  inactive controls. `PaneDecoration::refresh()` recursively searches native
  header/selector subtrees. Pane Resize/Show/LayoutRequest events can also invoke
  refresh directly, independently of controller coalescing.
- Every discovery calls `EditScopes::refresh()`, which recursively looks up its
  native QAction under main before testing whether Edit is active, even when the
  action is already retained.
- Docked Clips retains its gutter decoration. Its event handling checks page
  context and positions/raises the gutter and button on layout/show/resize events.
  The floating Lightbox allocation adapter is a separate mechanism: created on
  undocking, disabled/stopped on docking, with its application filter removed on
  the normal completed-start path.
- Nodes' 150 ms bounds timer exists only while floating and is deleted during
  docking cleanup. The activation bridge is also stopped before docking. The timer
  is a separate event-driven-cleanup candidate, not evidence for this docked report.
- Color tool selection preservation acts on page departure/return, not ordinary
  node selection. Window grouping is gated by window lifecycle/activation events;
  normal child focus changes do not directly reorder windows.

These are overhead candidates, not a confirmed explanation for multi-second stalls.

## Windows baseline

Passive public-Qt observation on Windows Resolve Studio 21.2.0.5 with internal
Unbound 2026.11.08.006 found no sustained selection stall. Three clip selections
and three node selections were measured with Clips/Nodes floating, then the same
counts with those two panes docked. Other tools remained floating to preserve the
user's unsaved layout: this is not an all-docked or uninstalled comparison.

- Floating selections: no watched child-creation/catalogued layout/show/parent
  events and no controller MetaCall deliveries in the one-second sample windows.
- Docked selections: one clip selection produced one child-added/polished event
  under `bigBossWidget` and one controller MetaCall; the other five selections
  produced none of the watched discovery events/callbacks.
- First application Paint delivery after press: clips 23–37 ms, nodes 0.5–9 ms.
  Largest inter-event gap while the event dispatcher was not sleeping: 71 ms
  across these selections. Subsequent paints continued asynchronously.
- Docking and native mode changes did produce the expected discovery events and
  controller callbacks, providing a positive control for the observer.

These timings are not render-completion times or measurements of Unbound's own
execution cost. A MetaCall count is not an exact discovery invocation count.
The observer neither intercepts input nor instruments function bodies. Windows
does not currently support the repeated-discovery hypothesis for these selections;
macOS-specific native event patterns and accessibility work remain untested.
The temporary observer was removed and Clips/Nodes returned to floating state.

## Next investigation

Use our own macOS Resolve installation, preferably 21.2, and a simple project.
Compare fresh all-docked startup, floating panes, and docked-again state. Select
clips and nodes separately. Measure counts and cumulative/max GUI-thread time for
discovery, branch indexing and each decoration refresh; include native event types
and repeated batches. Aggregate diagnostics rather than logging every event.

Compare a controlled diagnostic run without repeated header refresh, retaining
initial decoration, to establish causality. Then isolate Clips gutter work if
needed. Check for feedback from our own layout/visibility changes, not just the
cost of tree searches. Native process samples can distinguish Unbound work from
native layout/accessibility or rendering work invoked by it.

If confirmed, narrow discovery to relevant structural changes, cache known anchors
and the Scopes action, and refresh only invalidated pane headers. Preserve lazy
native header replacement, Metadata selection states, Color tool variants and
Single/Dual Screen reparenting. Do not blindly disable discovery after startup.

No production changes or performance claims follow from this code review alone.
