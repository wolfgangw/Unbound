# Automatic Lua activation

Normal Lua installation includes `Unbound.scriptlib` in the root of the user's
Resolve Fusion Scripts directory. It calls the private loader under
`~/Library/Application Support/Unbound/Runtime/` once and returns immediately.
No Unbound shortcut is installed in Workspace → Scripts. The hook owns no
controller, settings or UI logic and uses no polling loop or `fusion:Execute` call.
Contexts without native FFI return silently; only an FFI-capable invocation calls
the private loader. This avoids reporting a startup failure from an auxiliary Lua
context when native attachment has already succeeded. Capable-context failures
are caught and printed to the scripting console; native failures include the
diagnostic log location.

Blackmagic's [Fusion scripting guide, page 24](https://documents.blackmagicdesign.com/UserManuals/Fusion8_Scripting_Guide.pdf#page=24)
documents root `.scriptlib` execution at startup. See
[platform status](../../../../doc/platforms/macos.md) for compatibility limits.
The loader requires an existing Qt application; startup timing remains a native
integration constraint, not a reason to occupy Fusion's Lua executor with a loop.

Developer controls for disabling and restoring automatic startup:

```sh
infrastructure/macos/lua/startup/disable.sh
# With Resolve closed, restore activation using the matching runtime and hook:
infrastructure/macos/lua/startup/enable.sh /absolute/path/to/Lua-candidate
```

Disable moves only the hook to `~/Library/Application Support/Unbound/StartupBackups/`.
The runtime and settings remain; loaded native code stays until normal exit.
Enable delegates to the normal installer, keeping the runtime and hook together.
These are repository development commands, not Resolve menu items.

See the [native test checklist](../../../../tests/platforms/macos/README.md).
The LuaJIT fixture checks restricted globals, private paths, silent success and
contained failures. Installation fixtures cover migration, updates and rollback;
live checks still establish Resolve startup and native interaction compatibility.
