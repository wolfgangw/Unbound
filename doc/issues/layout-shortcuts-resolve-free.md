# Layout shortcuts in Resolve Free

## Problem and confirmed findings

On macOS Resolve Free, Unbound itself works, but its Workspace > Scripts > Unbound
actions and assigned keyboard shortcuts do not perform their commands.

The current scripts use `fusion:Execute(...)` to call the already-loaded Unbound
library inside Resolve. This console probe executes successfully:

```lua
fusion:Execute([[print("FFI:", type(ffi), "require:", type(require))]])
```

- Windows Resolve Studio: `FFI: table`, `require: function`.
- macOS Resolve Free: `FFI: nil`, `require: nil`.

The destination Lua context therefore lacks the facilities used by our native
command call. This does not establish whether the difference is edition-specific,
platform-specific or both. Working automatic startup demonstrates that its separate
Lua context can load Unbound. The probe does not describe the originating Workspace
script's environment.

## Keep the native integration

Preserve the eight scripts, Workspace submenu, Keyboard Customization entries,
user assignments and native conflict handling. Investigate only command delivery
from those scripts to the running Unbound instance. Reuse the existing shared command
API, GUI-thread dispatcher and admission/context safeguards.

## Next checks on macOS Free

1. Record the Resolve version and inspect capabilities directly in a Workspace
   script, separately from the `fusion:Execute` destination. Check available FFI,
   module loading and native scripting objects without assuming console and menu
   execution use the same context. Establish where command delivery stops.
2. Investigate whether available public scripting facilities offer a small native
   notification Unbound can observe through events. No suitable mechanism is yet
   established; stored values alone are insufficient without notification.
3. If the originating script supports native calls or a suitable transport, assess
   a narrowly scoped local IPC channel to the existing Unbound runtime. Loading
   Unbound into a separate script process does not reach Resolve's controller.

A local socket or named pipe is a fallback candidate, not a selected design. It
would need asynchronous reception, user/process endpoint ownership, access controls,
bounded validated messages and clean shutdown. Keep native transport behind platform
contracts and command behavior shared. No polling, separate service, persistent
script loop or keyboard interception. Preserve the working Studio delivery path
unless a tested common mechanism provides a clear benefit.

Do not change Resolve preferences or attempt to unlock restricted scripting APIs.
No implementation has been made for Free compatibility. Validate Workspace actions
and assigned shortcuts independently, including rapid repeats, page changes and
Resolve restart, once a delivery mechanism is established.

See [layout shortcut validation](layout-keyboard-shortcuts.md) and
[shared command architecture](../../AGENTS.md#script-commands).
