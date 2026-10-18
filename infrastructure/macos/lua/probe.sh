#!/bin/bash
# Read-only native loading probe for Resolve's Lua environment.
set -euo pipefail
root=$(cd "$(dirname "$0")/../../.." && pwd)
mkdir -p "$root/build/macos/lua"
xcrun clang -arch arm64 -mmacosx-version-min=11.0 -dynamiclib \
    "$root/tests/platforms/macos/loading/lua/nativeprobe.c" -o "$root/build/macos/lua/UnboundLuaProbe.dylib"
codesign --force --sign - "$root/build/macos/lua/UnboundLuaProbe.dylib"
printf "In Resolve's Lua console, run:\nassert(loadfile([[%s/tests/platforms/macos/loading/lua/probe.lua]]))([[%s]])\n" "$root" "$root"
