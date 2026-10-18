-- SPDX-License-Identifier: MIT
-- Private startup loader. Installed outside Resolve's Scripts menu directories.
local directory = assert(os and os.getenv and os.getenv("HOME"), "Cannot locate the user home directory")
    .. "/Library/Application Support/Unbound/Runtime"
local ffi = ffi or (require and require("ffi"))
assert(ffi and ffi.cdef and ffi.load, "Unbound requires native FFI in Resolve's internal Lua runtime")
ffi.cdef[[int UnboundLuaStart(void); int UnboundLuaStatus(void);]]
local loader = ffi.load(directory .. "/UnboundLuaLoader.dylib")
local result = loader.UnboundLuaStart()
if result < 0 then
    error("Unbound could not start (" .. result .. "). See ~/Library/Logs/Unbound/lua-bootstrap.log")
end
-- Native code retains both libraries before returning; no Lua global, polling
-- loop, project node, custom control window or script UIManager is required.
