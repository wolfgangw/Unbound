// SPDX-License-Identifier: MIT
#pragma once
#include <QCoreApplication>
#include <QEvent>
namespace Unbound {
// Native backends can consume a border press before Qt translates it. Report
// the start of a user move/resize separately from programmatic geometry changes.
class WindowInteractionEvent final : public QEvent {
public:
    static QEvent::Type eventType() {
        static const auto type=static_cast<QEvent::Type>(QEvent::registerEventType());
        return type;
    }
    static void notify(QObject *window) {
        if(!window) return;
        WindowInteractionEvent event;
        QCoreApplication::sendEvent(window,&event);
    }
private:
    WindowInteractionEvent() : QEvent(eventType()) {}
};
}
