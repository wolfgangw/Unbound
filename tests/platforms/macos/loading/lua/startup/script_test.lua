-- SPDX-License-Identifier: MIT
-- Exercise the actual startup and private loader scripts with a restricted runtime.
-- No installed files, native Unbound images or Resolve session are touched.
local root = assert(unboundTestSourceRoot)
local startup = root .. "/src/platforms/macos/loading/lua/startup/Unbound.scriptlib"
local loader = root .. "/src/platforms/macos/loading/lua/load.lua"
local home = "/fixture/User With Spaces"
local runtime = home .. "/Library/Application Support/Unbound/Runtime"
local function run(options)
    local starts, loads, messages = 0, 0, {}
    local env = { assert = assert, error = error, pcall = pcall, tostring = tostring }
    env.print = function(message) messages[#messages + 1] = message end
    env.os = { getenv = function(name) assert(name == "HOME"); return home end }
    env.ffi = {
        cdef = function(_) end,
        load = function(path)
            assert(path == runtime .. "/UnboundLuaLoader.dylib")
            return { UnboundLuaStart = function() starts = starts + 1; return options.result or 1 end }
        end,
    }
    env.loadfile = function(path)
        loads = loads + 1
        assert(path == runtime .. "/load.lua")
        if options.missing then return nil, "missing launcher" end
        return setfenv(assert(loadfile(loader)), env)
    end
    if options.noffi then env.ffi = nil end
    if options.incompleteffi then env.ffi.load = nil end
    if options.nohome then env.os = nil end
    local skipped = options.noffi or options.incompleteffi
    -- debug, io, require and package are intentionally absent, as in Resolve.
    assert(pcall(setfenv(assert(loadfile(startup)), env)), "Startup failure escaped into Resolve")
    if options.missing or skipped or options.nohome then assert(starts == 0)
    else assert(starts == 1) end
    assert(loads == ((skipped or options.nohome) and 0 or 1))
    local failed = not skipped and (options.missing or options.nohome or (options.result and options.result < 0))
    assert(#messages == (failed and 1 or 0), "Successful startup must be silent")
    if failed then
        assert(messages[1]:find("automatic startup failed", 1, true))
        assert(not messages[1]:find("Workspace", 1, true))
    end
    if skipped then
        -- Skipping an auxiliary context must not disable later capable startup.
        env.ffi = {cdef = function(_) end, load = function(_)
            return {UnboundLuaStart = function() starts = starts + 1; return 1 end}
        end}
        assert(pcall(setfenv(assert(loadfile(startup)), env)))
        assert(starts == 1 and loads == 1 and #messages == 0)
    end
end
run({})
run({result = 2})
run({result = 3})
run({result = -1})
run({result = -2})
run({missing = true})
run({noffi = true})
run({incompleteffi = true})
run({nohome = true})
print("PASS: startup returns after one private-loader call; restricted globals, spaced paths, active loader and failures handled")
