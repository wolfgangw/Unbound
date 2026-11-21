# Color viewer: open validation and space reclamation

Floating playback and interaction are user-validated on Windows.

## External Scopes in Color Cinema Viewer

On Windows with Resolve Studio 21.2 build 5, external Scopes follow playback
in Color Cinema Viewer. Entering on a still frame or stepping through frames
produces no new external plot. After playback stops, a repaint can clear the
last trace while leaving the graticule intact. All four plot widgets and their
OpenGL contexts remain valid. The same external Scopes track individual frames
on Edit in Cinema Viewer and on Color in the ordinary viewer.

Color Cinema's own **Show Scopes** context-menu overlay follows paused frames.
For four independent plots on the second display, maximize the ordinary floating
Color viewer on the first display (Win+Up). Resolve supplies current-frame
traces in that configuration. No public API for requesting a paused external
scope frame in Color Cinema has been identified.

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
