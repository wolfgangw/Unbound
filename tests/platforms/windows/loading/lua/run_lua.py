# SPDX-License-Identifier: MIT
"""Run a Lua 5.1 fixture script against Resolve's lua5.1.dll on Windows."""
import ctypes
import os
from pathlib import Path
import sys

library = Path(sys.argv[1]).resolve()
script = Path(sys.argv[2]).resolve()
with os.add_dll_directory(str(library.parent)):
    lua = ctypes.CDLL(str(library))
    lua.luaL_newstate.restype = ctypes.c_void_p
    lua.luaL_openlibs.argtypes = [ctypes.c_void_p]
    lua.luaL_loadstring.argtypes = [ctypes.c_void_p, ctypes.c_char_p]
    lua.lua_pcall.argtypes = [ctypes.c_void_p, ctypes.c_int, ctypes.c_int, ctypes.c_int]
    lua.lua_tolstring.argtypes = [ctypes.c_void_p, ctypes.c_int, ctypes.c_void_p]
    lua.lua_tolstring.restype = ctypes.c_char_p
    lua.lua_close.argtypes = [ctypes.c_void_p]
    state = lua.luaL_newstate()
    assert state
    try:
        lua.luaL_openlibs(state)
        result = lua.luaL_loadstring(state, script.read_bytes())
        if not result:
            result = lua.lua_pcall(state, 0, 0, 0)
        if result:
            message = lua.lua_tolstring(state, -1, None)
            raise RuntimeError(message.decode('utf-8', errors='replace') if message else 'Lua execution failed')
    finally:
        lua.lua_close(state)
