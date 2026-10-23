# Outstanding platform validation

Current architecture, supported behavior and tested configurations are in AGENTS.md.
This file tracks validation gaps, not build/session history.

- Native Intel Mac installation, startup and pane interaction remain unvalidated.
  Collect hardware, macOS, Resolve and Unbound versions with community reports.
- Windows Free Resolve needs validation of startup and pane behavior.
- macOS needs broader multi-display, Retina scaling, display-removal,
  Spaces/fullscreen and Fusion workflow coverage, plus native verification of
  page-return focus restoration and the current shared Color Clips adapter.
- Color Clips' new divider guide and docked gutter alignment need native visual
  confirmation. Exercise Nodes/Effects as well as the left sidebar, interruption,
  fresh-session restoration and page/layout transitions on each backend; fixture
  coverage does not establish all of Resolve's native behavior.
- Browser-download distribution checks and signing remain open: Windows code
  signing and macOS Developer ID/notarization. These are distribution limitations,
  not missing pane features.
