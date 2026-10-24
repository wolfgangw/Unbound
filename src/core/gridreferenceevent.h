// SPDX-License-Identifier: MIT
#pragma once
#include <QCoreApplication>
#include <QEvent>
#include <QVector>
#include <QRect>
namespace Unbound {
// One synchronous read when a Shift overlay opens; no persistent settings reader.
class GridReferenceEvent final : public QEvent {
public:
    QVector<QRect> rectangles;
    static QEvent::Type eventType() {
        static const auto type=static_cast<QEvent::Type>(QEvent::registerEventType());return type;
    }
    GridReferenceEvent():QEvent(eventType()) {}
    static QVector<QRect> request() {
        GridReferenceEvent event;if(qApp)QCoreApplication::sendEvent(qApp,&event);return event.rectangles;
    }
};
}
