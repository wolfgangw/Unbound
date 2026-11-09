# Resolve Free shortcut validation

## Current status

The shared native-action fallback is implemented. macOS Resolve Free automatically
selects it at startup through `IsStudio()`, and the user confirms the installed
build works. Menu and assigned-shortcut delivery were also verified independently
with Dock all. No manual Console activation is required.

Studio retains the existing Lua/Execute/FFI command path. Free retains the same
native scripts menu, keyboard assignments and conflict handling, while its Lua
payloads return quietly. The mechanism is documented in
[Script commands](../../AGENTS.md#script-commands).

On the tested macOS Free installation, both Workspace scripts and the Execute
context lack the native-call facilities used by the Studio path. Windows Studio
exposes them. Console/startup capabilities do not describe Workspace execution;
these observations alone do not isolate edition from platform differences.

The arm64 fixture suite and installer checks pass. Shared tests cover generated
commands for both platforms, edition routing, native action discovery, menu
rebuilding, removed actions, duplicate prevention and loader teardown.

## Windows Studio validation

Fresh-process startup confirms the Studio script route. The user confirms menu
actions and assigned shortcuts work, rapid layout commands remain stable without
delayed replay, and page changes produce the expected page-local results.
The Windows automated build checks also pass, including narrowed menu observation.

## Remaining platform coverage

- Windows Free fallback behavior remains unvalidated; test it if available.
- Native Intel Mac execution remains unvalidated.

See [layout shortcut validation](layout-keyboard-shortcuts.md) for broader command
coverage. Keep remaining validation here; the implementation belongs in AGENTS.md.
