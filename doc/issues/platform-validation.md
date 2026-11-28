# Outstanding platform validation

Current architecture, supported behavior and tested configurations are in AGENTS.md.
This file tracks validation gaps, not build/session history.

- Validate docked-button drag-to-undock on Windows/macOS across standard panes,
  Edit viewers, Color Timeline, Clips and native Curves/Warper/Scopes palettes:
  button anchoring without placement flash, continuous movement/snapping, saved
  size, early release, Escape, application switching and subsequent docking.
  Windows movement, stacking below pins and live Scopes are user-validated;
  verify native keyboard context on release (including Source and Scopes).
  Shared fixtures cover click threshold, transferred/replaced headers, deferred
  release, stacking and cleanup; broader native Resolve coverage remains open.
- Native Intel Mac installation, startup and pane interaction remain unvalidated.
  Collect hardware, macOS, Resolve and Unbound versions with community reports.
- With Unbound 0.7.2, Resolve Free 21.1.1 Build 10 starts Unbound on both
  Windows and macOS. The earliest confirmed failure is 21.1.2 Build 10 on
  Windows: Workspace > Scripts > Unbound appears, but the panel buttons are
  absent and neither Unbound DLL is loaded. Both platforms show the same
  menu and missing buttons on 21.2 Build 6. Windows startup traces show that
  21.1.1 searches the user and system Scripts roots and Utility folders for
  `.scriptlib` files, and Resolve itself reads the installed hook. In 21.2,
  Resolve still searches for `.lua` menu scripts. Neither startup on Color
  nor the first Fusion visit showed a `.scriptlib` search or a read of the
  root or composition hook. A supported startup path for the affected builds remains open.
- Broaden Always-on-top coverage for fullscreen/maximized panes and unusual
  native dialog/ownership combinations on both platforms, plus Windows Dual Screen.
  General macOS application switching, menus/dialogs, main minimize/restore and
  ordering against both Dual Screen windows are validated;
  see AGENTS.md.
- Remaining macOS pre-release checks:
  - Final installer update/uninstall/reinstall, settings preservation and startup.
  - Edit Program/Source viewer mode-exit and page-return compatibility; see
    [Dual Screen validation](dual-screen-floating-panels.md).
  - Floating Edit Source versus timeline transport needs separate confirmation.
    Confirmed startup/interaction results are recorded in AGENTS.md.
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
- Windows code signing and macOS Developer ID/notarization remain open. The unsigned/
  ad-hoc-signed download flow is supported; these are distribution limitations,
  not missing pane features.

- Verify Edit/Fairlight Metadata's empty-selection header in a fresh Resolve process
  on both platforms: dock/undock, title/background dragging and transitions between
  empty, single-clip and multiple-clip selection. The shared decoration uses the
  existing native header row; no fallback strip is added.
