# Outstanding platform validation

Current architecture, supported behavior and tested configurations are in AGENTS.md.
This file tracks validation gaps, not build/session history.

- Native Intel Mac installation, startup and pane interaction remain unvalidated.
  Collect hardware, macOS, Resolve and Unbound versions with community reports.
- Windows Free Resolve needs validation of startup and pane behavior.
- Per-pane "Always on top" still needs native macOS validation: ordinary/pinned
  activation, menus/dialogs, viewer/audition routing, page/layout recall and main
  minimize/restore, including maximized panes. Broaden Windows coverage for
  fullscreen, multi-display and unusual native dialog/ownership combinations.
- macOS needs broader multi-display, Retina scaling, display-removal,
  Spaces/fullscreen and Fusion workflow coverage, plus native verification of
  page-return focus restoration and the current shared Color Clips adapter.
- Color Clips' divider guide and docked gutter alignment still need Windows visual
  confirmation. macOS presentation and alignment are user-validated in the tested
  configuration. Broader coverage remains for all three sidebar dividers
  (left/Nodes/Effects), interruption, fresh-session restoration and page/layout
  transitions on each backend; fixture coverage does not establish all of
  Resolve's native behavior.
- macOS Shift-grid resizing is user-validated. Broader live Resolve coverage remains
  for stationary modifier changes, Option bypass, other-page outlines and release
  cleanup. Cocoa overlay properties and shared grid/reference calculations are
  fixture-validated; native Intel remains unvalidated.
- Browser-download distribution checks and signing remain open: Windows code
  signing and macOS Developer ID/notarization. These are distribution limitations,
  not missing pane features.
