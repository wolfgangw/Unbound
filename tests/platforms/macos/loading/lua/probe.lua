-- SPDX-License-Identifier: MIT
-- Run internally in Resolve's Lua console. Does not start Unbound or change widgets.
-- Resolve omits the debug library. Supply the repository path explicitly.
local root = assert(..., "Pass the absolute repository path to the loaded probe")
local function record(message) print(message) end
record("Lua: " .. tostring(_VERSION) .. "; JIT: " .. tostring(jit and jit.version))
record("Runtime: require=" .. type(require) .. "; package=" .. type(package)
    .. "; ffi=" .. type(ffi) .. "; os=" .. type(os)
    .. "; getenv=" .. type(os and os.getenv) .. "; io=" .. type(io)
    .. "; debug=" .. type(debug))
local ok, result = pcall(function()
    local ffi = ffi or (require and require("ffi"))
    assert(ffi and ffi.cdef and ffi.load, "Native FFI loading is unavailable")
    record("FFI available")
    ffi.cdef[[int UnboundLuaProbePid(void); int UnboundLuaProbeMainThread(void); int UnboundLuaProbeQt(void);]]
    local probe = ffi.load(root .. "/build/macos/lua/UnboundLuaProbe.dylib")
    record("Native load succeeded; pid=" .. probe.UnboundLuaProbePid()
        .. "; main_thread=" .. probe.UnboundLuaProbeMainThread() .. "; qt_application=" .. probe.UnboundLuaProbeQt())
end)
if not ok then record("PROBE FAILED: " .. tostring(result)) end
