#!/bin/bash
set -euo pipefail
stage=${1:?Expected Lua candidate directory}
source "$(dirname "$0")/../architectures.sh"
mac_architectures "${2:-universal}"
[[ -f "$stage/runtime/load.lua" && -f "$stage/startup/Unbound.scriptlib" ]]
for binary in "$stage/runtime/UnboundLuaLoader.dylib" "$stage/runtime/UnboundHelper.dylib"; do
    mac_verify_architectures "$binary"
    for architecture in "${MAC_ARCHS[@]}"; do
    otool -arch "$architecture" -L "$binary"
    while IFS= read -r dependency; do
        case "$dependency" in
            /usr/lib/*|/System/Library/Frameworks/*|@rpath/UnboundHelper.dylib|@rpath/UnboundLuaLoader.dylib) ;;
            @rpath/libQt5Core.5.dylib|@rpath/libQt5Gui.5.dylib|@rpath/libQt5Widgets.5.dylib)
                [[ "$binary" == */UnboundHelper.dylib ]] || { echo 'Lua loader links Qt prematurely' >&2; exit 1; } ;;
            *) echo "Unexpected dependency: $dependency" >&2; exit 1 ;;
        esac
    done < <(otool -arch "$architecture" -L "$binary" | tail -n +2 | sed -E 's/^[[:space:]]*//; s/ \(compatibility version.*//')
    done
done
