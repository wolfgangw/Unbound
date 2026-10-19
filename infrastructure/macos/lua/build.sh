#!/bin/bash
# Build, validate and sign a Lua startup candidate; public packaging is explicit.
set -euo pipefail
root=$(cd "$(dirname "$0")/../../.." && pwd)
version=$(head -n 1 "$root/VERSION" | tr -d '\r\n')
[[ "$version" =~ ^[0-9]+\.[0-9]+\.[0-9]+$ ]] || { echo 'Expected a semantic VERSION.' >&2; exit 1; }
[[ $(uname -s) == Darwin ]] || { echo 'Requires macOS' >&2; exit 1; }
public=0
if [[ $# == 1 && "$1" == --release ]]; then public=1
elif [[ $# != 0 ]]; then echo 'Usage: build.sh [--release]' >&2; exit 1; fi
variant=${UNBOUND_MAC_ARCHS:-universal}
source "$root/infrastructure/macos/architectures.sh"
mac_architectures "$variant"
if [[ $public == 1 ]]; then
    [[ "$variant" == universal ]] || { echo 'Public Mac candidates must be universal.' >&2; exit 1; }
    [[ ! -e "$root/release/mac/lua/$version" ]] || { echo 'Public candidate already exists; refusing to overwrite.' >&2; exit 1; }
fi
build="$root/build/macos/lua/$variant"
cmake_archs=$(IFS=';'; echo "${MAC_ARCHS[*]}")
cmake -S "$root" -B "$build" -DCMAKE_BUILD_TYPE=Release \
    "-DCMAKE_OSX_ARCHITECTURES=$cmake_archs" -DCMAKE_OSX_DEPLOYMENT_TARGET=11.0 \
    -DUNBOUND_QT_SOURCE="${UNBOUND_QT_SOURCE:-$root/build/macos/qtbase-source}" \
    -DUNBOUND_RESOLVE_APP="${UNBOUND_RESOLVE_APP:-/Applications/DaVinci Resolve/DaVinci Resolve.app}"
cmake --build "$build" --parallel
"$root/infrastructure/macos/lua/test.sh" "$build" | tee "$build/validation.log"
"$root/infrastructure/macos/lua/audit.sh" "$build/stage" "$variant"
stamp=$(date +%Y.%m.%d)
mkdir -p "$root/release/mac/lua"
counter=1
while :; do
    if [[ $public == 1 ]]; then name=$version
    else printf -v name '%s.%03d' "$stamp" "$counter"; fi
    destination="$root/release/mac/lua/$name"
    if mkdir "$destination" 2>/dev/null; then break; fi
    [[ $public == 0 ]] || { echo 'Public candidate already exists; refusing to overwrite.' >&2; exit 1; }
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
printf '%s\n' "${MAC_ARCHS[@]}" > "$destination/ARCHITECTURES"
cp "$build/validation.log" "$destination/validation.log"
git -C "$root" status --short > "$destination/source-status.txt"
printf '%s\n' "$destination"
