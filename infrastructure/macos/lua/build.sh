#!/bin/bash
# Build, validate and sign an internal Lua startup candidate.
set -euo pipefail
root=$(cd "$(dirname "$0")/../../.." && pwd)
version=$(head -n 1 "$root/VERSION" | tr -d '\r\n')
[[ $(uname -s) == Darwin && $(uname -m) == arm64 ]] || { echo 'Requires an Apple Silicon Mac' >&2; exit 1; }
build="$root/build/macos/lua/native"
cmake -S "$root" -B "$build" -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_OSX_ARCHITECTURES=arm64 -DCMAKE_OSX_DEPLOYMENT_TARGET=11.0 \
    -DUNBOUND_QT_SOURCE="${UNBOUND_QT_SOURCE:-$root/build/macos/qtbase-source}" \
    -DUNBOUND_RESOLVE_APP="${UNBOUND_RESOLVE_APP:-/Applications/DaVinci Resolve/DaVinci Resolve.app}"
cmake --build "$build" --parallel
"$root/infrastructure/macos/lua/test.sh" "$build"
"$root/infrastructure/macos/lua/audit.sh" "$build/stage"
stamp=$(date +%Y.%m.%d)
mkdir -p "$root/release/mac/lua"
counter=1
while :; do
    printf -v name '%s.%03d' "$stamp" "$counter"
    destination="$root/release/mac/lua/$name"
    if mkdir "$destination" 2>/dev/null; then break; fi
    [[ -d "$destination" ]] || { echo "Cannot create $destination" >&2; exit 1; }
    counter=$((counter+1))
done
cp -R "$build/stage/runtime" "$destination/"
cp -R "$build/stage/startup" "$destination/"
for library in UnboundLuaLoader.dylib UnboundHelper.dylib; do
    codesign --force --sign - "$destination/runtime/$library"
    codesign --verify --strict "$destination/runtime/$library"
done
git -C "$root" rev-parse HEAD > "$destination/source-head.txt"
printf '%s\n' "$version" > "$destination/VERSION"
printf '%s\n' "$name" > "$destination/BUILD"
git -C "$root" status --short > "$destination/source-status.txt"
printf '%s\n' "$destination"
