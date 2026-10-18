#!/bin/bash
set -euo pipefail
stage=${1:?Expected Lua candidate directory}
[[ -f "$stage/runtime/load.lua" && -f "$stage/startup/Unbound.scriptlib" ]]
for binary in "$stage/runtime/UnboundLuaLoader.dylib" "$stage/runtime/UnboundHelper.dylib"; do
    [[ $(lipo -archs "$binary") == arm64 ]] || { echo "Unexpected architecture: $binary" >&2; exit 1; }
    otool -L "$binary"
    while IFS= read -r dependency; do
        case "$dependency" in
            /usr/lib/*|/System/Library/Frameworks/*|@rpath/UnboundHelper.dylib|@rpath/UnboundLuaLoader.dylib) ;;
            @rpath/libQt5Core.5.dylib|@rpath/libQt5Gui.5.dylib|@rpath/libQt5Widgets.5.dylib)
                [[ "$binary" == */UnboundHelper.dylib ]] || { echo 'Lua loader links Qt prematurely' >&2; exit 1; } ;;
            *) echo "Unexpected dependency: $dependency" >&2; exit 1 ;;
        esac
    done < <(otool -L "$binary" | tail -n +2 | sed -E 's/^[[:space:]]*//; s/ \(compatibility version.*//')
done
