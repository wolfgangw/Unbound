# SPDX-License-Identifier: MIT
"""Run the actual startup hook in Resolve's LuaJIT, outside Resolve."""
import ctypes
import os
from pathlib import Path
import shutil
import sys
import tempfile

resolve = Path(os.environ.get('UNBOUND_RESOLVE_DIR', r'C:\Program Files\Blackmagic Design\DaVinci Resolve'))
root = Path(__file__).resolve().parents[5]
loader = Path(sys.argv[1]).resolve()
with os.add_dll_directory(str(resolve)), tempfile.TemporaryDirectory(prefix='unbound-lua-') as temporary:
    runtime = Path(temporary) / 'Unicode ä 日本' / 'Unbound' / 'Runtime'
    runtime.mkdir(parents=True)
    shutil.copy2(loader, runtime / loader.name)
    os.environ['LOCALAPPDATA'] = str(runtime.parent.parent)
    os.environ['UNBOUND_LOG_FILE'] = str(Path(temporary) / 'fixture.log')
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
    hook = (root / 'src/platforms/windows/loading/lua/startup/Unbound.scriptlib').read_text(encoding='utf-8')
    script = '''ffi=require('ffi')
local messages={}
print=function(text) messages[#messages+1]=text end
local function startup()
''' + hook + '''
end
startup()
assert(#messages==1 and messages[1]:find('startup rejected',1,true), 'Unicode load or host rejection failed')
messages={}; ffi=nil; startup(); assert(#messages==0, 'Non-FFI context did not skip silently')
'''
    result = lua.luaL_loadstring(state, script.encode('utf-8'))
    if not result:
        result = lua.lua_pcall(state, 0, 0, 0)
    failure = lua.lua_tolstring(state, -1, None) if result else None
    lua.lua_close(state)
    if result:
        raise RuntimeError(failure)
print('PASS: actual Windows LuaJIT startup, Unicode runtime path, contained host rejection and non-FFI context')
