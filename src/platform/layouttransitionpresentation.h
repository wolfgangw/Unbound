// SPDX-License-Identifier: MIT
#pragma once
#include <QtGlobal>
#include <QList>
#include <functional>
#include <memory>
class QWidget;
namespace Unbound {
// Optional visual protection for a shared layout transaction. Implementations
// may use snapshots or another presentation mechanism; core owns readiness,
// cancellation and lifetime. No backend may request capture permission.
class LayoutTransitionPresentation {
public:
    virtual ~LayoutTransitionPresentation() = default;
    // Interrupt on actual OS application departure; transient Qt activation
    // changes during native content transfer are not sufficient.
    // Existing popup-level windows remain live above the presentation and must
    // not be baked into a snapshot. Failure leaves the normal layout usable.
    virtual bool begin(QWidget *owner,std::function<void()> interrupted,
        const QList<QWidget *> &interactiveWindows={}) = 0;
    virtual qint64 retainedBytes() const = 0;
    virtual void hold() = 0;
    virtual void reveal(int milliseconds,std::function<void(bool)> finished) = 0;
    virtual void cancel() = 0;
};
// Explicitly unsupported presentation is a successful normal layout change,
// with no capture, visual delay, callback or permission request.
class NoLayoutTransitionPresentation final : public LayoutTransitionPresentation {
public:
    bool begin(QWidget *,std::function<void()>,const QList<QWidget *> & = {}) override {return false;}
    qint64 retainedBytes() const override {return 0;}
    void hold() override {}
    void reveal(int,std::function<void(bool)>) override {}
    void cancel() override {}
};
std::unique_ptr<LayoutTransitionPresentation> makeLayoutTransitionPresentation();
}
