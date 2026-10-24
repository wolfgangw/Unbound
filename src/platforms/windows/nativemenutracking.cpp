// SPDX-License-Identifier: MIT
#include "nativemenutracking.h"
namespace Unbound {
std::unique_ptr<NativeMenuTracking> observeNativeMenuTracking(std::function<void()>) {
    // Windows menu interaction already reaches the shared Qt outside-input
    // filter; there is no separate system menu-bar tracking bridge to install.
    return {};
}
}
