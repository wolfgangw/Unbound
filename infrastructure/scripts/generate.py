#!/usr/bin/env python3
"""Generate native Resolve menu commands from the shared command catalogue."""
import argparse
import json
from pathlib import Path

parser = argparse.ArgumentParser()
parser.add_argument('--platform', choices=['windows', 'macos'], required=True)
parser.add_argument('--output', type=Path, required=True)
parser.add_argument('--header', type=Path)
args = parser.parse_args()
root = Path(__file__).resolve().parents[2]
commands = json.loads((root / 'src/scripts/commands.json').read_text())
windows = '''ffi.cdef[[
    void *GetModuleHandleW(const uint16_t *);
    void *GetProcAddress(void *, const char *);
]]
local name = "Unbound.dll"
local wide = ffi.new("uint16_t[?]", #name + 1)
for i = 1, #name do wide[i-1] = name:byte(i) end
local library = ffi.C.GetModuleHandleW(wide)
assert(library ~= nil, "Unbound is not running.")
local entry = ffi.C.GetProcAddress(library, "UnboundExecuteCommand")
assert(entry ~= nil, "Please update Unbound to use its script commands.")
'''
macos = '''ffi.cdef[[
    void *dlopen(const char *, int);
    void *dlsym(void *, const char *);
    int dlclose(void *);
]]
local path = assert(os.getenv("HOME"), "Cannot locate Unbound.")
    .. "/Library/Application Support/Unbound/Runtime/Unbound.dylib"
-- RTLD_NOW | RTLD_NOLOAD: never initialize another copy from a command.
local library = ffi.C.dlopen(path, 0x12)
assert(library ~= nil, "Unbound is not running.")
local entry = ffi.C.dlsym(library, "UnboundExecuteCommand")
if entry == nil then
    ffi.C.dlclose(library)
    error("Please update Unbound to use its script commands.")
end
'''
args.output.mkdir(parents=True, exist_ok=True)
entries = []
for command in commands:
    name = command['label'] + '.lua'
    arguments = json.dumps(command['arguments'], separators=(',', ':'))
    body = '-- SPDX-License-Identifier: MIT\n-- Generated Unbound command; shortcut assignment belongs to Resolve.\n'
    body += 'local ffi = ffi or (require and require("ffi"))\n'
    body += 'assert(ffi, "Unbound commands require Resolve Lua FFI.")\n'
    body += windows if args.platform == 'windows' else macos
    body += 'local execute = ffi.cast("int (*)(unsigned int, const char *, const char *)", entry)\n'
    body += f'local result = execute(1, "{command["command"]}", [=[{arguments}]=])\n'
    if args.platform == 'macos':
        body += 'ffi.C.dlclose(library)\n'
    body += 'assert(result >= 1, "Unbound command rejected (" .. result .. ").")\n'
    # Workspace scripts run in a separate script process. Execute the small
    # native call in Resolve through its existing Fusion scripting connection.
    body = ('-- SPDX-License-Identifier: MIT\n'
            '-- Generated Unbound command; shortcut assignment belongs to Resolve.\n'
            'local app = fusion or fu\n'
            'if not app and resolve then app = resolve:Fusion() end\n'
            'if not app and bmd then app = bmd.scriptapp("Fusion") end\n'
            'assert(app, "Cannot connect to Resolve.")\n'
            'app:Execute([==[\n' + body + ']==])\n')
    (args.output / name).write_text(body, encoding='utf-8', newline='\n')
    entries.append((name, body))
if args.header:
    header = '#pragma once\n#include <map>\n#include <string>\n'
    header += 'inline std::map<std::wstring,std::string> unboundCommandScripts() { return {\n'
    header += ',\n'.join(f'{{L"{name}",R"UNBOUND({body})UNBOUND"}}' for name, body in entries)
    header += '\n}; }\n'
    args.header.write_text(header, encoding='utf-8', newline='\n')
