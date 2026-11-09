# SPDX-License-Identifier: MIT
"""Validate edition routing in both generated payloads using a Lua 5.1 runner."""
import json
from pathlib import Path
import subprocess
import sys
import tempfile

root = Path(__file__).resolve().parents[3]
library, *runner = sys.argv[1:]
with tempfile.TemporaryDirectory(prefix='unbound-script-routing-') as directory:
    temporary = Path(directory)
    for platform in ('macos', 'windows'):
        output = temporary / platform
        subprocess.run([sys.executable, str(root / 'infrastructure/scripts/generate.py'),
                        '--platform', platform, '--output', str(output)], check=True)
        for script in output.glob('*.lua'):
            fixture = r'''
local deliveries = 0
local fakeffi = {
    cdef=function() end,
    new=function() return {} end,
    cast=function() return function(version, command, arguments)
        assert(version==1 and command:find('layout.',1,true)==1)
        deliveries=deliveries+1
        return 1
    end end,
    C={GetModuleHandleW=function() return 1 end, GetProcAddress=function() return 1 end,
       dlopen=function() return 1 end, dlsym=function() return 1 end, dlclose=function() end}
}
local outer=assert(loadfile(SCRIPT))
local function run(edition, viaFusion)
    local nativeHost={IsStudio=function() if edition=="error" then error("Unavailable") end; return edition end}
    local app={Execute=function(self,payload)
        local env={assert=assert,error=error,pcall=pcall,ffi=fakeffi,os={getenv=function() return '/fixture' end}}
        if viaFusion then env.fusion={GetResolve=function() return nativeHost end}
        else env.resolve=nativeHost end
        if edition==false then env.ffi=nil end -- Free must never need FFI.
        setfenv(assert(loadstring(payload)),env)()
    end}
    setfenv(outer,{assert=assert,fusion=app})()
end
run(false,false);run(false,true)
assert(deliveries==0,'Free Lua path dispatched alongside native fallback')
run(true,false);run(true,true)
assert(deliveries==2,'Studio command delivery changed')
run(nil,false)
assert(deliveries==3,'Unknown edition did not retain existing route')
run('error',false)
assert(deliveries==4,'Edition query failure broke existing route')
'''.replace('SCRIPT', json.dumps(str(script)))
            path = temporary / 'routing.lua'
            path.write_text(fixture)
            subprocess.run([*runner, library, str(path), str(root)], check=True)
print('PASS: both platforms, all eight scripts, Free without FFI, Studio and unknown-edition routing')
