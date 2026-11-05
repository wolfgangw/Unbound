# Layout keyboard shortcuts

Status: feasibility investigated on Windows; implementation pending.

## Verified findings

- Resolve discovers Utility scripts under
  `%APPDATA%/Blackmagic Design/DaVinci Resolve/Support/Fusion/Scripts/Utility/`
  and lists them in Workspace > Scripts and Keyboard Customization.
- A temporary no-op Lua script appeared in Keyboard Customization with its native
  shortcut editor. The user independently verified assignment, search and conflict
  notification with `ww-test-api`.
- An ordinary Qt QAction added to Workspace did not enter Keyboard Customization.
  No documented command/category registration API was found in the installed
  Scripting or Workflow Integration documentation. Keyboard preset APIs manage
  presets, not command registration.
- Scripts provide a promising native assignment/dispatch route without an Unbound
  keyboard interception layer. No production integration or end-to-end Unbound
  shortcut execution has been implemented. Temporary probes were removed without
  changing keyboard assignments.

## Proposed integration

Ship small Utility script entry points with stable filenames and an `Unbound`
prefix. Forward requests to shared GUI-thread commands used by both scripts and
the existing menu; retain native modal, page and lifecycle guards. Keep
`Unbound.scriptlib` in the Scripts root as the automatic startup library.

| Command | Intended behavior on the current page |
| --- | --- |
| Recall layout 1–5 | Recall the corresponding menu slot |
| Dock all | Invoke the existing Dock all operation |
| Save active layout | Update the active layout; clean layouts need no action |
| Save as new layout | Use the existing naming flow when a slot is free |

Names/assignments must remain independent of user layout names. Confirm slot
identity and deletion/reordering semantics before implementation. Do not assign
default shortcuts or overwrite user mappings.

Use brief nonmodal feedback for an empty slot, no active layout, five occupied
slots or an unsupported page. Save-as must not silently overwrite or invent a name.
Exact feedback presentation and script-to-core entry point remain to be designed.

## Next validation

Start with one recall command and Dock all, then extend to all eight commands.

- Verify execution with the Unbound menu closed, from docked/floating panes and
  text fields, preserving Resolve's normal shortcut precedence.
- Verify command identity and assignments across restart, script additions/removal
  and ordering changes, installation updates and keyboard preset reload.
- Validate macOS discovery, assignment and dispatch; Windows evidence alone does
  not establish cross-platform behavior.
- Exercise page changes, unavailable slots, naming cancellation, full capacity,
  modal dialogs and requests arriving during layout restoration.

The native Scripts category is the candidate integration point; a dedicated
top-level Unbound category is not established as feasible.
