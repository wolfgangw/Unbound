# Color Clips and Lightbox

Clips remains unsupported. Older saved Clips/Lightbox floating intent is deliberately
discarded by settings migration. The initial Git import already contained that
exclusion; no retained explanation establishes the earlier experiment's failure.

The complete native Color snapshot and a targeted live read-only comparison establish:

- `m_pLightBox` (`UiLightBoxImp`) is a child of `m_pColorTimeLineForm`, alongside
  `timelineFrame`. Its sole layout child is `frameLightBoxBorder`.
- Clips and the page-filling Lightbox reuse the same `m_pLightBox`,
  `frameLightBoxBorder`, `thumbnailView` and `vsrWidget` instances. Their parent
  relationships remain unchanged during the observed mode transition.
- Lightbox increases their allocation from a short horizontal strip to a tall
  thumbnail grid and reveals `topBorderFrame`. That toolbar is hidden in Clips mode.
- In the live comparison, the renderer minimum height changed from 134 in Clips
  mode to 0 in Lightbox; the thumbnail container's size hint also changed from
  `(0,134)` to `(0,0)`. Mode changes affect sizing policy as well as allocation.
- The renderer and thumbnail container are custom Resolve widgets. An unbounded
  Qt maximum size does not establish that floating height would produce grid rows
  while Resolve remains in ordinary Clips mode.
- The visible Clips strip has no dedicated header or guaranteed empty corner.
  An overlay could cover clip identification/selection or scrolling. The hidden
  Lightbox toolbar is not a valid always-visible anchor.

The user reports that only thumbnails select clips; clicking their surrounding
decoration does nothing. This describes selection behavior, not a proposed button
location: the user explicitly excludes placing our button in clip decoration.
Clip information must remain readable as thumbnails scroll or change size.
The captured subtree exposes thumbnails and
their decoration through one custom `vsrWidget`, not individual Qt thumbnail or
label widgets. An overlay therefore cannot use named decoration widgets as anchors.
Do not treat the whole renderer as an unused-background drag surface.

The native Clips visibility controls are `btnToggleTimelineClips` and
`btnToggleTimelineClipsInDSM`; `btnColorToggleLightbox` controls the alternate mode.
These are distinct from `btnColorToggleTimeline`. The existing shared Color
session accepts the one-child Clips layout and can transfer `frameLightBoxBorder`
while retaining `m_pLightBox`. Its generic recovery watches transferred-content
Hide/ParentChange events and native visibility toggles; it does not track Lightbox
mode or mode-dependent sizing. Thus compatibility with Lightbox remains a live
validation requirement, not an established failure of reparenting.

The current session had other Unbound panels floating; these captures establish
native mode behavior of the still-docked Clips subtree, not a fresh no-Unbound
baseline. Both targeted captures were complete. The full native GammaRay snapshot
was captured separately without Unbound. No production Clips support was added.

## Reversible reparenting experiment

A temporary GUI-thread helper transferred `frameLightBoxBorder` into a plain Qt
window, preserving the native outer widget/layout and inserting a placeholder.
This exercised the transfer used by the shared Color mechanism without registering
Clips, persisting state or installing automatic recovery. Closing the host, a
bounded timeout or an explicit second invocation restored native ownership.

- Clip selection updated the main viewer and grading controls. Horizontal scrolling
  worked. Turning off the native Clips toggle reclaimed main-window space while
  the transferred content remained visible in its host.
- A taller host did not produce rows in ordinary Clips mode: the horizontal strip
  remained centered with unused space above and below.
- Entering Lightbox switched the floating content to its native grid and revealed
  its toolbar. Enlarging the host produced multiple columns/rows. Resolve also
  switched the main page to its full Lightbox allocation, leaving the placeholder
  filling that area. Floating the content alone does not prevent this page change.
- Exiting Lightbox returned the floating content to the Clips strip. Returning
  content to its native layout and re-enabling Clips restored the original view.
  Original selection was restored. Timeline and Nodes were floating during this
  experiment; this is not full integration or page-switch validation.

## Proposed integration

The desired floating presentation is a wrapping grid using the original native
renderer, without Lightbox controls and with thumbnail sizing independent of
Lightbox zoom. A horizontal strip alone does not meet the requested feature.
Button placement remains undecided; use a reserved area if no suitable native
anchor exists, never overlay clip decoration. The agreed Lightbox behavior is to
suspend floating state for native Lightbox and resume it on exit, retaining
floating geometry/intent.
This needs explicit mode tracking and must not compete with generic content
recovery, layout recall or persistence. Do not silently disable Lightbox or force
its mode merely to obtain a grid. Test context menus, page switches, native toggle
variants and docking/restoration with the production session before enabling it.

If support is added, avoid resurrecting obsolete experimental floating intent merely
by registering the old persistence key. Decide migration behavior explicitly.

## Renderer controls and wrapping checks

Targeted Qt metadata inspection found writable integer properties on
`UiVSRWidget`: thumbnail width/height, top/bottom label-bar dimensions, grid
margins/spacing and ruler dimensions. No wrapping or grid-mode property or slot
was exposed by the renderer or its thumbnail container. Metadata inspection did
not call arbitrary custom property getters.

A separate reversible experiment changed only known thumbnail width/height
properties from 128x72 to 180x100 and removed the renderer's explicit minimum
height, with vertical size policies set to Ignored on renderer/container. The
dimensions changed visually and read back correctly; the content remained one
horizontal row and its internal strip allocation clipped the enlarged content.
Restoring the properties, constraints, policies and original layout restored the
native strip. Thus thumbnail sizing is locally adjustable, but those controls do
not enable wrapping or make the renderer's internal allocation follow host height.

Read-only inspection of this Resolve binary identified a private presentation
path which selects an internal layout mode and recalculates thumbnail rows and
columns. This supports a mode distinction below QWidget layout policy. That path
is not exposed in the inspected Qt metadata. Its binary addresses and object
offsets are build-specific diagnostic evidence, not a usable shared backend API.
No private-address calls, native memory writes or binary patches were performed.

`UiLightBoxImp::OnShowOnLightBox(bool)` is not a verified grid-mode setter. Its
implementation writes global state and has display-related side effects; the
surrounding native UI includes `LightBoxShowOnSDI`. It was not invoked as a
speculative local-mode control. A reliable local mode hook remains unresolved;
successful wrapping in a normal Color page has not been demonstrated.

## Current stopping point

The user additionally observed that resizing the native main-window Lightbox
changes thumbnail size in discrete steps as well as wrapping: narrower windows
produce smaller thumbnails, wider windows larger ones. This has not yet been
measured with property captures. The hypothesis is that Lightbox zoom specifies
a preferred size and the renderer adjusts actual sizing to its column allocation;
the observation alone does not establish that algorithm.

The next useful comparison is to keep Lightbox zoom unchanged and capture the
renderer thumbnail dimensions, spacing, geometry and constraints on both sides
of those size transitions. If exposed dimensions change, investigate their update
path; if they remain constant, the adaptive sizing occurs below those properties.
Do not assume hiding Lightbox controls yields stable thumbnail sizing.

Investigation is paused with no production Clips support. Independent thumbnail
dimensions and reversible content transfer are demonstrated; automatic wrapping
in normal Color mode and independence from Lightbox's adaptive sizing remain
unresolved. Native Lightbox entry should temporarily return Clips to its native
slot, with the floating arrangement resumed on exit. That integration, page-switch
behavior, saved-state handling and button placement remain unimplemented. No
button should cover clip decoration. Temporary mutation experiments were restored;
their diagnostic DLLs remain loaded until Resolve exits, without active experiments.
