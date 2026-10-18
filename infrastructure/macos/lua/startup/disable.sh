#!/bin/bash
# Disable future automatic activation; a running native helper stays until exit.
set -euo pipefail
user_root=${UNBOUND_USER_HOME:-$HOME}
scripts="$user_root/Library/Application Support/Blackmagic Design/DaVinci Resolve/Fusion/Scripts"
hook="$scripts/Unbound.scriptlib"
[[ -f "$hook" ]] || { echo 'Automatic startup is not enabled.'; exit 0; }
backup="$user_root/Library/Application Support/Unbound/StartupBackups"
mkdir -p "$backup"
destination=$(mktemp -d "$backup/disabled.XXXXXX")
mv "$hook" "$destination/Unbound.scriptlib"
printf 'Disabled automatic startup; hook retained at %s/Unbound.scriptlib\n' "$destination"
