# Color viewer: open validation and space reclamation

The mechanism is documented in AGENTS.md. Floating playback and interaction are
user-validated on Windows. Viewer-space reclamation is explicitly deferred.

Open work:

- Validate Cinema/expanded-viewer transitions, page return, layout recall and
  fresh-session restoration.
- Validate floating viewer together with floating Clips, including Lightbox
  transitions and docking either component in either order.
- Validate macOS image surfaces and behavior in a fresh native session.

Space reclamation is deferred by product decision: retain the native allocation
and floating placeholder. No layout redesign is planned.

Use object-name discovery and reversible Qt layout ownership throughout. No
private Resolve layout calls, binary offsets or persistent polling.
