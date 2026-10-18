#!/bin/bash
set -euo pipefail
root=$(cd "$(dirname "$0")/../../.." && pwd)
build=${1:-"$root/build/macos/lua/native"}
resolve=${UNBOUND_RESOLVE_APP:-'/Applications/DaVinci Resolve/DaVinci Resolve.app'}
export QT_QPA_PLATFORM_PLUGIN_PATH="$resolve/Contents/PlugIns/platforms"
export QT_QPA_PLATFORM=cocoa
export UNBOUND_LOG_FILE="$build/test-startup.log"
"$build/src/platforms/macos/unbound-lua-script-test" \
    "$resolve/Contents/Libraries/Fusion/libluajit-5.1.2.dylib" \
    "$root/tests/platforms/macos/loading/lua/startup/script_test.lua" "$root"
python3 "$root/tests/platforms/macos/loading/lua/install_test.py"
loader="$build/stage/runtime/UnboundLuaLoader.dylib"
helper="$build/stage/runtime/UnboundHelper.dylib"
"$build/src/platforms/macos/unbound-lua-loader-test" "$loader"
"$build/src/platforms/macos/unbound-lua-loader-test" "$loader" --existing-controller
binary="$build/src/platforms/macos/pane-float-smoke"
"$build/src/platforms/macos/UnboundLayoutTests"
for mode in --mac-window-test --mac-main-resize-test --mac-floating-resize-test --mac-main-snap-test --mac-main-drag-test --mac-placement-test --mac-layout-test --header-button-test --inspector-title-test \
    --settings-json-test --nodes-canvas-test --main-placement-test --reclaim-test \
    --fairlight-test --standalone-test --shutdown-test --close-all-test; do
    "$binary" "$mode" "$helper"
done
"$binary"
