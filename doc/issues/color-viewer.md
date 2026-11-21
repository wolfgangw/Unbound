# Color viewer: open validation and space reclamation

Floating playback and interaction are user-validated on Windows.

## External Scopes in Color Cinema Viewer

On Windows with Resolve Studio 21.2 build 5, Color Cinema Viewer's **Hide Scopes
on Play** also affects floating Scopes on a second display. With it checked,
the external plots follow playback. With it unchecked, they show only their
grids during playback. Both results hold with **Show Scopes** either on or off.

Entering on a still frame or stepping through frames produces no new external
plot. None of the four menu combinations restored updates on paused seeks.
After playback stops, a repaint can clear the
last trace while leaving the graticule intact. All four plot widgets and their
OpenGL contexts remain valid. The same external Scopes track individual frames
on Edit in Cinema Viewer and on Color in the ordinary viewer.

Color Cinema's own **Show Scopes** context-menu overlay follows paused frames.
For four independent plots on the second display, maximize the ordinary floating
Color viewer on the first display (Win+Up). Resolve supplies current-frame
traces in that configuration. No public API for requesting a paused external
scope frame in Color Cinema has been identified.

## macOS Cinema entry

First entry from a floating Color viewer has been reported behind the workspace
and floating panes. Fresh-session reproduction and stacking diagnosis remain open.
Only a single display is available for local macOS validation.

## Open validation

- Validate Cinema/expanded-viewer transitions, page return, layout recall and
  fresh-session restoration.
- Validate floating viewer together with floating Clips, including Lightbox
  transitions and docking either component in either order.
- Validate macOS image surfaces and behavior in a fresh native session.

Space reclamation is deferred by product decision: retain the native allocation
and floating placeholder. No layout redesign is planned.

Use object-name discovery and reversible Qt layout ownership throughout. No
private Resolve layout calls, binary offsets or persistent polling.
