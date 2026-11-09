# SPDX-License-Identifier: MIT
"""Run generated Workspace scripts in LuaJIT with an isolated application context."""
import ctypes
import os
from pathlib import Path
import subprocess
import sys
import tempfile

root = Path(__file__).resolve().parents[5]
resolve = Path(os.environ.get('UNBOUND_RESOLVE_DIR', r'C:\Program Files\Blackmagic Design\DaVinci Resolve'))
helper = Path(sys.argv[1]).resolve()
with os.add_dll_directory(str(resolve)), tempfile.TemporaryDirectory(prefix='unbound-commands-') as temporary:
    subprocess.run([sys.executable, str(root / 'infrastructure/scripts/generate.py'),
                    '--platform', 'windows', '--output', temporary], check=True)
    runtime = ctypes.CDLL(str(helper))
    lua = ctypes.CDLL(str(resolve / 'lua5.1.dll'))
    lua.luaL_newstate.restype = ctypes.c_void_p
    lua.luaL_openlibs.argtypes = [ctypes.c_void_p]
    lua.luaL_loadstring.argtypes = [ctypes.c_void_p, ctypes.c_char_p]
    lua.lua_pcall.argtypes = [ctypes.c_void_p, ctypes.c_int, ctypes.c_int, ctypes.c_int]
    lua.lua_tolstring.argtypes = [ctypes.c_void_p, ctypes.c_int, ctypes.c_void_p]
    lua.lua_tolstring.restype = ctypes.c_char_p
    lua.lua_close.argtypes = [ctypes.c_void_p]
    state = lua.luaL_newstate()
    assert state
    lua.luaL_openlibs(state)
    scripts = list(Path(temporary).glob('*.lua'))
    assert len(scripts) == 8
    try:
        for path in scripts:
            # The command environment has no FFI or require. Only Execute can
            # reach the native environment, which resolves the real DLL export.
            # With no QApplication, the real entry must reject safely with -1.
            script = '''
local nativeffi = require('ffi')
local calls = 0
local app = {Execute=function(self, payload)
    calls = calls + 1
    local native = assert(loadstring(payload))
    setfenv(native, {ffi=nativeffi, assert=assert, pcall=pcall})
    local ok, err = pcall(native)
    assert(not ok and tostring(err):find('Unbound command rejected (-1).', 1, true), tostring(err))
end}
local command = assert(loadstring([===[
''' + path.read_text(encoding='utf-8') + '''
]===]))
for _, connection in ipairs({{fusion=app}, {fu=app},
    {resolve={Fusion=function() return app end}},
    {bmd={scriptapp=function(name) assert(name=='Fusion'); return app end}}}) do
    connection.assert=assert
    setfenv(command,connection)
    command();command()
end
assert(calls==8, 'Command did not use the native scripting connection exactly once')
setfenv(command,{assert=assert})
local ok,err=pcall(command)
assert(not ok and tostring(err):find('Cannot connect to Resolve.',1,true))
'''
            result = lua.luaL_loadstring(state, script.encode('utf-8'))
            if not result:
                result = lua.lua_pcall(state, 0, 0, 0)
            if result:
                raise RuntimeError(lua.lua_tolstring(state, -1, None))
    finally:
        lua.lua_close(state)
print('PASS: eight Workspace scripts, isolated native execution, connection variants, repeated calls and missing connection')
