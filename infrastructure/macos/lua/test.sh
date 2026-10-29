#!/bin/bash
set -euo pipefail
root=$(cd "$(dirname "$0")/../../.." && pwd)
build=${1:-"$root/build/macos/lua/${UNBOUND_MAC_ARCHS:-universal}"}
source "$root/infrastructure/macos/architectures.sh"
resolve=${UNBOUND_RESOLVE_APP:-'/Applications/DaVinci Resolve/DaVinci Resolve.app'}
export QT_QPA_PLATFORM_PLUGIN_PATH="$resolve/Contents/PlugIns/platforms"
export QT_QPA_PLATFORM=cocoa
python3 "$root/tests/platforms/macos/loading/lua/install_test.py"
python3 "$root/tests/platforms/macos/architecture_test.py" "$build/stage"
for architecture in $(lipo -archs "$build/stage/runtime/UnboundHelper.dylib"); do
if ! mac_test_architecture "$architecture"; then
    continue
fi
echo "Testing $architecture on $(uname -m) host (Intel on Apple Silicon uses Rosetta)."
export UNBOUND_LOG_FILE="$build/test-startup-$architecture.log"
run_native() { /usr/bin/arch "-$architecture" "$@"; }
run_native "$build/src/platforms/macos/unbound-lua-script-test" \
    "$resolve/Contents/Libraries/Fusion/libluajit-5.1.2.dylib" \
    "$root/tests/platforms/macos/loading/lua/startup/script_test.lua" "$root"
loader="$build/stage/runtime/UnboundLuaLoader.dylib"
helper="$build/stage/runtime/UnboundHelper.dylib"
run_native "$build/src/platforms/macos/unbound-lua-loader-test" "$loader"
run_native "$build/src/platforms/macos/unbound-lua-loader-test" "$loader" --existing-controller
run_native "$build/src/platforms/macos/audition-activation-test"
binary="$build/src/platforms/macos/pane-float-smoke"
run_native "$build/src/platforms/macos/UnboundLayoutTests"
run_native "$build/src/platforms/macos/UnboundLayoutTests" --index-header-drag
run_native "$build/src/platforms/macos/UnboundLayoutTests" --color-clips-startup
run_native "$build/src/platforms/macos/UnboundLayoutTests" --auxiliary-page-context
for mode in --mac-menu-tracking-test --mac-resize-input-test --mac-grid-test --mac-presentation-test --mac-window-test --mac-main-resize-test --mac-floating-resize-test --mac-main-snap-test --mac-main-drag-test --mac-placement-test --mac-layout-test --header-button-test --inspector-title-test \
    --settings-json-test --nodes-canvas-test --main-placement-test --reclaim-test \
    --fairlight-test --standalone-test --shutdown-test --close-all-test; do
    run_native "$binary" "$mode" "$helper"
done
run_native "$binary"
echo "PASS: $architecture fixture suite on $(uname -m) host."
done
