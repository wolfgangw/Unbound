# Fred-In-Progress

Design discussion: adopting Resolve's native floating Media Pool windows.
“Fred” is our working name for a window created by a bin's **Open As New Window**
context-menu action. No production integration has been implemented yet.

## Observed behavior

- The action is available in Edit, Color and Fairlight Media Pools.
- A Fred is a fully interactive additional view of the shared Media Pool. Bin
  edits/additions/removals synchronize in both directions; users can navigate to
  other bins. It is not visibly restricted to its originating bin.
- A Fred opened on Edit remains visible and usable on other Resolve pages.
- It has native header actions, a leftmost close control and independent moving
  and resizing. Activating it through the taskbar currently raises only Fred.
- Inspection of the live Windows “Timelines” Fred identified a top-level
  `UiMp::UiMediaPoolViewsWidgetImp`, object name `UiMediaPoolViewsWidgetImp`,
  without a Qt parent or native HWND owner. Its header is
  `frameMediaPoolTitleBarTop`, with `btnMediaPoolOptions` and
  `btnMediaPoolDialogClose`. These are compatible with our existing Media Pool
  header-anchor approach. The corresponding macOS structure needs verification.

Shared content does not prove that there is no internal bin identity. We have
not established a reliable way to read/restore the selected bin or create a new
Fred programmatically through Resolve's existing actions.

## Intended interaction

- Add our button beside the native options action.
- Click closes Fred through Resolve's normal close handling. There is no original
  dock slot to restore; close is the agreed equivalent action here. Retain the
  native close control and use an accurate tooltip, e.g. “Click to close; drag to move”.
- Drag moves Fred; right-click exposes the existing layout menu.
- Include Fred in grouping and snapping: activating it raises the visible,
  non-minimized Resolve group, and activating another member includes Fred.
  Preserve the selected window's focus. Group raising remains current policy;
  whether users should eventually be able to raise just one pane is a separate,
  deferred product question.
- Freds remain global, with identical positions and sizes across page switches.
  Do not assign them to the page from which they were opened.

## Least intrusive integration

Prefer adopting the existing Resolve-created window rather than moving its content
into another Unbound host. Resolve retains content ownership, native behavior,
close handling and lifetime; Unbound adds window-management behavior.

Existing undocked panes also retain Resolve-owned content, but need our host
because they originate inside main. Fred already has a host. Rehosting is possible
in principle, but adds parenting, teardown and close-path risks without an evident
benefit. Resolve's audition command context depends on widget relationships;
preserving native ancestry avoids introducing another context correction.

Detection can use the application-level Qt Show event, checking the recognized
top-level identity and setting up once after construction. One startup inventory
finds already-open Freds; destruction removes registrations. No recurring polling
or repeated full-tree scans are needed. Validate identity and expected header
structure rather than relying on the user-visible bin/window title.

Current grouping and drag code assumes main or an Unbound `PaneFloatingWindow`.
Adoption therefore needs an explicit way to register native windows and reuse
interaction/snapping behavior without that host-type requirement. Detection and
header insertion look straightforward; native movement/resizing and both backend
integrations still require implementation and testing. This could support other
native floating windows later, but should not become a speculative framework.

## Persistence and named layouts

The user's expectation is that named layouts capture Freds, primarily their count
and geometry. Restoring selected bins would be convenient, but is optional: users
can cheaply navigate to the intended bins themselves. Multiple Freds may be useful
for large projects with deeply nested bins.

Design alternatives:

1. **One persistent global arrangement, outside named layouts.** Simple and avoids
   page-layout recalls moving shared windows. Remembers count/geometry across
   sessions, but does not satisfy the user's expectation that named layouts capture
   the whole arrangement. Useful as a staging step, not the preferred final model.
2. **Separate global named layouts.** Explicitly separates page-local and global
   state, but introduces another layout category and coordination burden. No desire
   to add that UI complexity at present.
3. **Each page layout also snapshots the global Fred arrangement.** Preferred
   direction. Recall restores its page-local panes plus the saved
   global Fred set; that Fred arrangement then persists across page switches.
   Recalling another page's layout may consequently move, create or close Freds.
   The user considers this manageable: people deliberately using multiple Freds
   can prepare compatible page layouts. Ordinary page switching must not itself
   replace the global arrangement with each page's stored snapshot.
4. **Rewrite every stored layout whenever a Fred changes.** Technically feasible
   for these small records, but makes every layout share an overwritten arrangement
   instead of preserving independent snapshots. It conflicts with our explicit
   save/update rule and can destroy arrangements users intended to recall. This
   is incompatible with the intended independent snapshots.

Proposed state model for option 3 (still to be finalized):

- Maintain one live global Fred arrangement and persist it for session restoration.
- Include a copy in a named layout only on explicit save/update.
- Recall replaces the live global arrangement along with page-local state.
- Changes can make active layouts on multiple pages dirty relative to their saved
  snapshots. Recompute/coalesce comparisons as needed; do not rewrite snapshots.
- Debounce persistence during geometry changes and capture exact final geometry
  after interaction. Comparing small lists of identities and rectangles is cheap;
  creating/closing native windows is the more significant lifecycle operation.

No performance measurements have been made for this feature. There is no apparent
need for expensive work on every mouse move or for modifying every saved layout.

## Open questions and next investigation

- Can we reliably request creation of the required number of Freds through an
  existing Resolve action, without private memory access or fragile UI automation?
  Restoring geometry of an existing Fred is much simpler than recreating missing ones.
- How should we identify/match multiple Freds across recalls and sessions? Their
  titles and selected bins can change; neither should be assumed to be a stable ID.
- Can selected bins be observed and restored using a stable, minimally intrusive
  mechanism? If not, restore windows/count/geometry and leave bin selection to users.
- Define handling of extra/missing windows, close cancellation if applicable,
  project changes, unavailable creation actions and partial restoration failures.
- Define migration for old layouts without Fred data: absence must have deliberate
  semantics, distinct from an explicitly saved empty Fred arrangement if necessary.
- Verify native creation, header identity, focus, group activation, move/resize,
  snapping and close behavior on both Windows and macOS.
- Confirm dirty-state/menu semantics for global windows and avoid capturing a
  half-restored arrangement during layout recall.

Next work should establish native creation/restoration feasibility, then settle
the snapshot semantics before implementing persistence. Preserve the existing
page-local pane model and explicit layout-update policy throughout.
