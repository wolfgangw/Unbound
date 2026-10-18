#!/bin/bash
# Install/update automatic Lua activation, preserving settings.
set -euo pipefail
candidate=${1:?Usage: install.sh /absolute/path/to/Lua-candidate}
[[ "$candidate" == /* && -f "$candidate/runtime/load.lua" && -f "$candidate/startup/Unbound.scriptlib" ]] || {
    echo 'Expected a Lua candidate with runtime/ and startup/.' >&2; exit 1;
}
if pgrep -x Resolve >/dev/null; then echo 'Close Resolve normally before installing Unbound.' >&2; exit 1; fi
for library in UnboundLuaLoader.dylib UnboundHelper.dylib; do
    codesign --verify --strict "$candidate/runtime/$library"
done
user_root=${UNBOUND_USER_HOME:-$HOME}
support="$user_root/Library/Application Support/Unbound"
scripts="$user_root/Library/Application Support/Blackmagic Design/DaVinci Resolve/Fusion/Scripts"
runtime="$support/Runtime"
hook="$scripts/Unbound.scriptlib"
mkdir -p "$support/InstallationBackups" "$scripts"
transaction=$(mktemp -d "$support/InstallationBackups/install.XXXXXX")
# Stage and verify everything before moving an existing installation.
cp -R "$candidate/runtime" "$transaction/runtime"
# Install the candidate's identity with its runtime, within the same transaction.
# Historical candidates without metadata remain installable but have no known version.
if [[ -f "$candidate/VERSION" && -f "$candidate/BUILD" ]]; then
    cp "$candidate/VERSION" "$candidate/BUILD" "$transaction/runtime/"
fi
cp "$candidate/startup/Unbound.scriptlib" "$transaction/Unbound.scriptlib"
for library in UnboundLuaLoader.dylib UnboundHelper.dylib; do
    codesign --verify --strict "$transaction/runtime/$library"
done
cmp "$candidate/runtime/load.lua" "$transaction/runtime/load.lua"
cmp "$candidate/startup/Unbound.scriptlib" "$transaction/Unbound.scriptlib"
# Recheck after staging in case Resolve was started during verification.
if pgrep -x Resolve >/dev/null; then echo 'Resolve started during installation; existing files were left in place.' >&2; exit 1; fi
saved_count=0
published_count=0
originals=()
backups=()
published=()
committed=0
rollback() {
    status=$?
    trap - EXIT
    if [[ $committed == 0 ]]; then
        for ((i=published_count-1;i>=0;i--)); do
            mv "${published[i]}" "$transaction/failed-$i" || echo "Could not retain failed installation: ${published[i]}" >&2
        done
        for ((i=saved_count-1;i>=0;i--)); do
            mv "${backups[i]}" "${originals[i]}" || echo "Restore manually: ${backups[i]} -> ${originals[i]}" >&2
        done
        echo "Installation failed; previous files restored where possible. Backup: $transaction" >&2
    fi
    exit "$status"
}
trap rollback EXIT
trap 'exit 130' INT
trap 'exit 143' TERM
save_previous() {
    if [[ -e "$1" || -L "$1" ]]; then
        mv "$1" "$transaction/$2"
        originals[saved_count]="$1"
        backups[saved_count]="$transaction/$2"
        saved_count=$((saved_count+1))
    fi
}
save_previous "$hook" previous-Unbound.scriptlib
save_previous "$runtime" previous-runtime
# Migrate the old menu-based installation out of Resolve's scan tree entirely.
save_previous "$scripts/Utility/Unbound.lua" previous-Unbound.lua
save_previous "$scripts/Utility/UnboundLua" previous-UnboundLua
mv "$transaction/runtime" "$runtime"
published[published_count]="$runtime"; published_count=$((published_count+1))
mv "$transaction/Unbound.scriptlib" "$hook"
published[published_count]="$hook"; published_count=$((published_count+1))
committed=1
printf 'Installed automatic Unbound startup. Open Resolve normally.\nBackup: %s\n' "$transaction"
