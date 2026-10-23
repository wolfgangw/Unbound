// SPDX-License-Identifier: MIT
#pragma once
#include <QList>
#include <functional>
#include <memory>
class QWidget;
namespace Unbound {
// Supplement a Qt backing-store freeze when native child surfaces composite
// independently. Completion always runs on the GUI thread. Destruction cancels
// pending preparation and removes any presentation retained by this instance.
class WindowPresentationHold {
public:
    virtual ~WindowPresentationHold() = default;
    virtual void begin(const QList<QWidget *> &windows, std::function<void(bool)> ready) = 0;
    virtual void release() = 0;
};
std::unique_ptr<WindowPresentationHold> makeWindowPresentationHold();
}
