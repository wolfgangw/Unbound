#!/bin/bash
# Shared macOS build mechanics; never sourced by the installed runtime.
mac_architectures() {
    case "${1:-universal}" in
        universal) MAC_ARCHS=(arm64 x86_64) ;;
        arm64|x86_64) MAC_ARCHS=("$1") ;;
        *) echo 'Architecture must be universal, arm64 or x86_64.' >&2; return 1 ;;
    esac
    MAC_ARCH_FLAGS=()
    for architecture in "${MAC_ARCHS[@]}"; do MAC_ARCH_FLAGS+=(-arch "$architecture"); done
}
mac_verify_architectures() {
    local binary=$1 actual expected
    actual=$(lipo -archs "$binary" | tr ' ' '\n' | sort | tr '\n' ' ')
    expected=$(printf '%s\n' "${MAC_ARCHS[@]}" | sort | tr '\n' ' ')
    [[ "$actual" == "$expected" ]] || { echo "Unexpected architectures in $binary: $actual (expected $expected)" >&2; return 1; }
}
mac_test_architecture() {
    # Intel hosts cannot execute arm64. Apple Silicon can validate Intel code
    # through Rosetta. Untestable slices require an explicit opt-in and remain
    # recorded as NOT RUN; a failed fixture itself is never skipped.
    if /usr/bin/arch "-$1" /usr/bin/true 2>/dev/null; then return 0; fi
    if [[ ${UNBOUND_MAC_ALLOW_UNTESTED:-0} == 1 ]]; then
        echo "NOT RUN: $1 execution unavailable on this host; explicitly allowed untested slice."
        return 1
    fi
    echo "Cannot execute $1 tests. UNBOUND_MAC_ALLOW_UNTESTED=1 explicitly permits an untested slice." >&2
    exit 1
}
