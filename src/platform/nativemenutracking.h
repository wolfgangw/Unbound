// SPDX-License-Identifier: MIT
#pragma once
#include <functional>
#include <memory>
namespace Unbound {
// Supplement Qt dismissal with native menu tracking and actual app departure. GUI-thread
// only; destruction stops observation. The callback may destroy the observer.
class NativeMenuTracking {
public:
    virtual ~NativeMenuTracking() = default;
};
std::unique_ptr<NativeMenuTracking> observeNativeMenuTracking(std::function<void()> started);
// Qt can report transient inactivity while Resolve reparents native content.
bool nativeApplicationActive();
}
