#!/bin/bash
# Remove only the per-user Lua installation. Always retain settings and layouts.
set -euo pipefail
if [[ $# != 0 ]]; then echo 'Usage: uninstall.sh' >&2; exit 1; fi
if pgrep -x Resolve >/dev/null; then echo 'Close DaVinci Resolve normally before uninstalling Unbound.' >&2; exit 1; fi
user_root=${UNBOUND_USER_HOME:-$HOME}
support="$user_root/Library/Application Support/Unbound"
scripts="$user_root/Library/Application Support/Blackmagic Design/DaVinci Resolve/Fusion/Scripts"
# Stage removals on the same volume, outside Resolve's script scan tree. If a
# move fails, restore previous files before reporting failure.
parent="$user_root/Library/Application Support"
[[ -d "$parent" ]] || { echo 'Unbound is not installed.'; exit 0; }
transaction=$(mktemp -d "$parent/.unbound-uninstall.XXXXXX")
saved_count=0
originals=()
backups=()
committed=0
rollback() {
    status=$?
    trap - EXIT
    if [[ $committed == 0 ]]; then
        for ((i=saved_count-1;i>=0;i--)); do
            mv "${backups[i]}" "${originals[i]}" || echo "Restore manually: ${backups[i]} -> ${originals[i]}" >&2
        done
        echo "Uninstall failed; previous files restored where possible. Recovery folder: $transaction" >&2
    fi
    exit "$status"
}
trap rollback EXIT
trap 'exit 130' INT
trap 'exit 143' TERM
stage_removal() {
    if [[ -e "$1" || -L "$1" ]]; then
        mv "$1" "$transaction/$2"
        originals[saved_count]="$1"
        backups[saved_count]="$transaction/$2"
        saved_count=$((saved_count+1))
    fi
}
stage_removal "$scripts/Unbound.scriptlib" hook
stage_removal "$support/Runtime" runtime
stage_removal "$scripts/Utility/Unbound.lua" legacy-loader
stage_removal "$scripts/Utility/UnboundLua" legacy-runtime
stage_removal "$support/InstallationBackups" installation-backups
stage_removal "$support/StartupBackups" startup-backups
# Only the explicitly staged Unbound files are removed, never the containing
# Scripts/support directories or unrelated installations.
committed=1
rm -rf -- "$transaction"
# Remove the product directory only if empty; retain all unrelated files.
rmdir "$support" 2>/dev/null || true
echo 'Unbound removed. Settings and layouts kept.'
