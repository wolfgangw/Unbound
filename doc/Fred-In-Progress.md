# Native bin windows: proposed integration

"Fred" denotes a native Media Pool window created through **Open As New Window**.
Integration is not implemented. Establish supported creation/restoration before
implementing persistence.

## Native behavior and identity

The action is available in Edit, Color and Fairlight. Each window is an interactive
view of the shared Media Pool: edits synchronize, navigation is not restricted to
the originating bin, and the window remains available across pages.

On Windows it is a top-level `UiMp::UiMediaPoolViewsWidgetImp`, object name
`UiMediaPoolViewsWidgetImp`, without a Qt parent or HWND owner. Its header is
`frameMediaPoolTitleBarTop`, with `btnMediaPoolOptions` and
`btnMediaPoolDialogClose`. Verify this structure on macOS. Titles and selected
bins are mutable and must not be assumed to provide stable identity.

## Intended behavior and architecture

Adopt Resolve's existing window, preserving ownership, ancestry, native content,
close handling and lifetime. Add the Unbound button beside options: click invokes
native close, drag moves, and right-click opens the layout menu. Use "Click to
close; drag to move" because there is no dock slot. Retain the native close control.

Include adopted windows in grouping and snapping while preserving focus. They
remain global, with the same geometry across page switches. Detection uses a
startup inventory and recognized top-level Show/destruction events, without
polling. Grouping and gestures need explicit support for adopted native windows
rather than assuming `PaneFloatingWindow` hosts.

## Proposed persistence

Named layouts should capture the global window count and geometry alongside
page-local panes. Selected-bin restoration is optional if no stable mechanism
exists. The preferred model, still to be finalized:

- Persist one live global arrangement for session restoration.
- Copy it into a named layout only on explicit save/update.
- Explicit recall restores both page-local and global arrangements. Ordinary
  page switching leaves global windows unchanged.
- Global changes can dirty active layouts on multiple pages; never rewrite their
  snapshots implicitly. Coalesce comparisons and geometry persistence.

## Open questions

- Supported creation of missing windows through existing native actions.
- Stable matching across recalls/sessions and optional selected-bin restoration.
- Extra/missing windows, close cancellation, project changes and partial failures.
- Migration semantics for absent data versus an explicitly empty arrangement.
- Dirty-state/menu behavior while global restoration is in progress.
- Native creation, focus, group activation, gestures, snapping and closing on both
  platforms.
