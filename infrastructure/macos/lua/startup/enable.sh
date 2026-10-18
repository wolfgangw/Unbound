#!/bin/bash
# Developer convenience: restore automatic activation using the normal installer.
# There is no separately installed manual/menu launcher.
set -euo pipefail
root=$(cd "$(dirname "$0")/.." && pwd)
exec "$root/install.sh" "${1:?Usage: enable.sh /absolute/path/to/Lua-candidate}"
