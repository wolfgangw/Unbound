#!/bin/bash
# Configure development headers; Resolve supplies every Qt runtime library.
set -euo pipefail
root=$(cd "$(dirname "$0")/../.." && pwd)
source_dir=${UNBOUND_QT_SOURCE:-"$root/build/macos/qtbase-source"}
revision=40143c189b7c1bf3c2058b77d00ea5c4e3be8b28
if [[ ! -d "$source_dir" ]]; then
    mkdir -p "$(dirname "$source_dir")"
    git clone --depth 1 --branch v5.15.2 https://github.com/qt/qtbase.git "$source_dir"
fi
[[ $(git -C "$source_dir" rev-parse HEAD) == "$revision" ]] || { echo 'Expected Qt v5.15.2 source revision' >&2; exit 1; }
cd "$source_dir"
if [[ ! -f src/corelib/global/qconfig.h ]]; then
    ./configure -opensource -confirm-license -release -nomake examples -nomake tests \
        -no-openssl -no-dbus -no-opengl QMAKE_MACOSX_DEPLOYMENT_TARGET=11.0
fi
perl bin/syncqt.pl -version 5.15.2 -outdir "$source_dir" "$source_dir"
echo 'Qt headers ready; no Qt libraries built or installed.'
