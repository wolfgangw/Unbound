#!/bin/bash
# macOS entry point: Lua startup loading.
set -euo pipefail
exec "$(dirname "$0")/lua/audit.sh" "$@"
