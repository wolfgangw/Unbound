# Layout keyboard shortcuts

Status: all eight actions confirmed on Windows through Resolve's native scripting
connection. Stable slots are user-validated. Button-pulse feedback and menu hover dismissal
await live validation. Remaining coverage below.

Resolve discovers `Scripts/Utility/Unbound/*.lua` as one Unbound submenu in
Workspace > Scripts and Keyboard Customization. Searching Unbound finds contained
commands without repeating the prefix in each filename. Windows discovery and
assignment UI are verified; the user confirmed native conflict notification with
an existing Utility script. Resolve sorts filenames alphabetically; clean names
are preferred over visible ordering prefixes.

Commands: Layout 1 through 5, Dock all, Save layout, Save as new layout. Slot numbers
remain stable across deletion; Save as new fills the lowest gap. Save as new uses existing generated names, not a naming dialog.
Workspace scripts run outside Resolve; public `fusion:Execute(...)` delivers the
native call into Resolve through the existing scripting connection. The implemented
interface and concurrency guard are documented in
[AGENTS.md](../../AGENTS.md#script-commands).

## Remaining validation

- Fresh Windows install/restart: menu commands and assigned shortcuts with the
  Unbound menu closed, from docked/floating panes and text fields. Preserve native
  shortcut precedence, focus and transport routing.
- Rapid repeats and mixed commands during restoration: no overlapping changes,
  deferred action backlog or saved intermediate layout. Check menu/shortcut mixing,
  page/app changes, modal dialogs and busy/unavailable feedback.
- Single green save pulse/red empty-or-full pulse on visible buttons only; preserve
  docking warnings and cancel on hide/app departure. Menu saves use the same pulses,
  with no status text; Save as new must create a new slot even when unchanged. Menu hover expiry and immediate
  outside-click dismissal.
- Empty slots, no active layout, five occupied slots, identical-configuration save and unsupported
  pages; verify Save changes only the active layout on the current page. Check
  middle-slot deletion, gap reuse, migration/backup and stable numbers after restart.
- Assignment persistence across restart, script additions/removal, ordering,
  installer update/uninstall and keyboard preset reload.
- Native macOS compilation, installation, script execution and the same lifecycle
  coverage. Installer shell fixtures on WSL do not establish native Mac behavior.

No user shortcut mappings have been assigned or modified by this work.
