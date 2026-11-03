-- SPDX-License-Identifier: MIT
local library, output = ...
assert(library and output, "Pass absolute library and output log paths")
assert(ffi and ffi.load, "Native FFI unavailable")
ffi.cdef[[int UnboundAuditionAudioTrace(const char *path);]]
UnboundAuditionAudioLibraries = UnboundAuditionAudioLibraries or {}
local probe = ffi.load(library)
UnboundAuditionAudioLibraries[library] = probe
assert(probe.UnboundAuditionAudioTrace(output)==1)
print("Passive audition trace started for three minutes. Close Console and compare docked/floating audio return.")
