#!/bin/bash
# Explicit installer packaging step; never changes the installed runtime.
set -euo pipefail
candidate=${1:?Usage: build.sh /absolute/path/to/Lua-candidate}
root=$(cd "$(dirname "$0")/../../.." && pwd)
[[ "$candidate" == /* && -f "$candidate/runtime/load.lua" && -f "$candidate/startup/Unbound.scriptlib" ]] || {
    echo 'Expected a private-runtime Lua candidate.' >&2; exit 1;
}
[[ -f "$candidate/VERSION" && -f "$candidate/BUILD" ]] || {
    echo 'Candidate lacks version metadata; rebuild it before packaging.' >&2; exit 1;
}
version=$(head -n 1 "$candidate/VERSION" | tr -d '\r\n')
runtime_build=$(head -n 1 "$candidate/BUILD" | tr -d '\r\n')
[[ "$version" =~ ^[0-9]+\.[0-9]+\.[0-9]+$ && ( "$runtime_build" == "$version" || "$runtime_build" =~ ^[0-9]{4}\.[0-9]{2}\.[0-9]{2}\.[0-9]{3,}$ ) ]] || {
    echo 'Invalid candidate version metadata.' >&2; exit 1;
}
source "$root/infrastructure/macos/architectures.sh"
mac_architectures "${UNBOUND_MAC_ARCHS:-universal}"
"$root/infrastructure/macos/lua/audit.sh" "$candidate" "${UNBOUND_MAC_ARCHS:-universal}"
for library in UnboundLuaLoader.dylib UnboundHelper.dylib; do codesign --verify --strict "$candidate/runtime/$library"; done
build="$root/build/macos/installer"
mkdir -p "$build" "$root/release/mac/installer"
python3 "$root/infrastructure/installer/generate.py" "$root/src/installer/dialog.json" "$build/installer-ui.generated.h"
stamp=$(date +%Y.%m.%d)
counter=1
while :; do
    printf -v name '%s.%03d' "$stamp" "$counter"
    destination="$root/release/mac/installer/$name"
    if mkdir "$destination" 2>/dev/null; then break; fi
    [[ -d "$destination" ]] || exit 1
    counter=$((counter+1))
done
app_name="Unbound $version"
if [[ "$runtime_build" != "$version" ]]; then app_name+=" ($runtime_build)"; fi
app="$destination/$app_name.app"
resources="$app/Contents/Resources"
mkdir -p "$app/Contents/MacOS" "$resources/payload" "$resources/scripts/startup"
xcrun clang++ -std=c++17 -fobjc-arc "${MAC_ARCH_FLAGS[@]}" -mmacosx-version-min=11.0 -framework AppKit \
    -I"$build" \
    "$root/src/platforms/macos/installer/main.mm" -o "$app/Contents/MacOS/UnboundInstaller"
sed -e "s/@VERSION@/$version/g" -e "s/@BUILD@/$(date +%Y%m%d).$counter/g" \
    -e "s/@RUNTIME_BUILD@/$runtime_build/g" \
    -e "s/@APP_NAME@/$app_name/g" \
    -e "s/@PACKAGE_BUILD@/$name/g" \
    "$root/src/platforms/macos/installer/Info.plist.in" > "$app/Contents/Info.plist"
cp -R "$candidate/runtime" "$candidate/startup" "$resources/payload/"
cp "$candidate/VERSION" "$candidate/BUILD" "$resources/payload/"
cp "$root/infrastructure/macos/lua/install.sh" "$root/infrastructure/macos/lua/uninstall.sh" "$resources/scripts/"
cp "$root/infrastructure/macos/lua/startup/enable.sh" "$root/infrastructure/macos/lua/startup/disable.sh" "$resources/scripts/startup/"
cp "$root/src/assets/unbound-128.png" "$resources/"
cp "$root/LICENSE" "$resources/"
iconset="$build/Unbound.iconset"
mkdir -p "$iconset"
for size in 16 32 128 256; do cp "$root/src/assets/unbound-$size.png" "$iconset/icon_${size}x${size}.png"; done
cp "$root/src/assets/unbound-32.png" "$iconset/icon_16x16@2x.png"
cp "$root/src/assets/unbound-64.png" "$iconset/icon_32x32@2x.png"
cp "$root/src/assets/unbound-256.png" "$iconset/icon_128x128@2x.png"
sips -z 512 512 "$root/src/assets/unbound-256.png" --out "$iconset/icon_512x512.png" >/dev/null
cp "$iconset/icon_512x512.png" "$iconset/icon_256x256@2x.png"
sips -z 1024 1024 "$root/src/assets/unbound-256.png" --out "$iconset/icon_512x512@2x.png" >/dev/null
iconutil -c icns "$iconset" -o "$resources/Unbound.icns"
plutil -lint "$app/Contents/Info.plist"
python3 "$root/tests/platforms/macos/loading/lua/install_test.py" --scripts-dir "$resources/scripts"
# Sign the enclosing app after copying its signed native payload and scripts.
codesign --force --sign - "$app"
codesign --verify --deep --strict "$app"
# Exercise the real window with test-only assertions; the distributable app
# contains neither fixture command-line switches nor snapshot code.
fixture=$(mktemp -d "$build/ui.XXXXXX")
cp -R "$app" "$fixture/Unbound Installer Test.app"
test_app="$fixture/Unbound Installer Test.app"
xcrun clang++ -std=c++17 -fobjc-arc "${MAC_ARCH_FLAGS[@]}" -mmacosx-version-min=11.0 -framework AppKit \
    -DUNBOUND_INSTALLER_TEST -I"$root/tests/platforms/macos/installer" \
    -I"$build" \
    "$root/src/platforms/macos/installer/main.mm" -o "$test_app/Contents/MacOS/UnboundInstaller"
codesign --force --sign - "$test_app"
for architecture in "${MAC_ARCHS[@]}"; do
    if mac_test_architecture "$architecture"; then
        /usr/bin/arch "-$architecture" "$test_app/Contents/MacOS/UnboundInstaller" --smoke-test --snapshot "$build/window-$architecture.png"
    fi
done | tee "$destination/validation.log"
mac_verify_architectures "$app/Contents/MacOS/UnboundInstaller"
otool -L "$app/Contents/MacOS/UnboundInstaller"
cp "$candidate/source-head.txt" "$destination/runtime-source-head.txt"
printf '%s\n' "$candidate" > "$destination/runtime-candidate.txt"
printf '%s\n' "$name" > "$destination/BUILD"
git -C "$root" status --short > "$destination/source-status.txt"
printf '%s\n' "$app"
