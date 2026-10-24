// SPDX-License-Identifier: MIT
#pragma once
#include <functional>
#include <memory>
namespace Unbound {
// Supplement Qt outside-input delivery where the native menu bar bypasses it. GUI-thread
// only; destruction stops observation. The callback may destroy the observer.
class NativeMenuTracking {
public:
    virtual ~NativeMenuTracking() = default;
};
std::unique_ptr<NativeMenuTracking> observeNativeMenuTracking(std::function<void()> started);
}
