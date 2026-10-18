# Windows Lua loading

`UnboundLuaLoaderTests` runs outside Resolve with isolated logging. It checks
missing application rejection, retry, worker-thread requests, GUI-thread
attachment, loader retention after the caller releases its handle, repeated
entry and an existing controller.
The normal Windows build runs it with and without an existing controller.

`python tests/platforms/windows/loading/lua/script_test.py <loader.dll>` runs
the actual hook in the installed Resolve LuaJIT DLL in a separate process.
It uses a temporary Unicode runtime path and verifies contained rejection when
no Qt application exists, plus silent skipping in contexts without FFI.
`UNBOUND_RESOLVE_DIR` overrides Resolve's installation directory.

Live acceptance requires normal Resolve startup after installation.
Check `%LOCALAPPDATA%/Unbound/lua-bootstrap.log` for the new process's completed
controller attachment, then verify header buttons, docking, layouts, page changes
and Fusion controls. A console capability probe or fixture pass alone does not
establish automatic startup. Windows Free and Studio require separate validation.
