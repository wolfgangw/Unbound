// SPDX-License-Identifier: MIT
#include "windowpresentation.h"
namespace Unbound {
namespace {
// The shared Qt backing-store freeze already preserves Windows presentation.
class BackingStorePresentation final : public WindowPresentationHold {
public:
    void begin(const QList<QWidget *> &, std::function<void(bool)> ready) override { ready(true); }
    void release() override {}
};
}
std::unique_ptr<WindowPresentationHold> makeWindowPresentationHold()
{ return std::make_unique<BackingStorePresentation>(); }
}
