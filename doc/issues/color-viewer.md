# Color viewer: open validation and space reclamation

The mechanism is documented in AGENTS.md. Floating playback and interaction are
user-validated on Windows. Viewer-space reclamation is explicitly deferred.

Open work:

- Validate Cinema/expanded-viewer transitions, page return, layout recall and
  fresh-session restoration.
- Validate floating viewer together with floating Clips, including Lightbox
  transitions and docking either component in either order.
- Define and implement space reclamation. Color has no viewer visibility toggle;
  this initial build retains the native allocation and floating placeholder.
  Decide how remaining upper side panels and lower color controls should share
  the released region without imposing a new page layout engine.
- Validate macOS image surfaces and behavior in a fresh native session.

Use object-name discovery and reversible Qt layout ownership throughout. No
private Resolve layout calls, binary offsets or persistent polling.
