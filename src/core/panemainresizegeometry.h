// SPDX-License-Identifier: MIT
#pragma once
#include "geometryjson.h"
#include "panegeometrytransaction.h"

namespace Unbound {
// Resolve's responsive main-window layout also changes floating descendants'
// native constraints. Remember the pre-resize intention across main resizes;
// Qt may temporarily grow a host, but that growth is not a user size choice.
// Only bounded transactions do correction work. This dormant event observer
// neither polls nor forces smaller native controls.
class PaneMainResizeGeometry final : public QObject {
public:
    PaneMainResizeGeometry(QWidget *host,QWidget *main,std::function<void(const QByteArray &)> restore)
        : QObject(host),m_host(host),m_main(main),m_restore(std::move(restore)) {
        qApp->installEventFilter(this);
    }
    void reset(const QByteArray &geometry={}) { ++m_generation; m_requested=geometry; m_transaction=nullptr; }
    QByteArray intendedGeometry(const QByteArray &fallback) const {
        return m_requested.isEmpty()?fallback:m_requested;
    }
protected:
    bool eventFilter(QObject *object,QEvent *event) override {
        if(object==m_host && (event->type()==WindowInteractionEvent::eventType()
                || event->type()==QEvent::WindowStateChange)) reset();
        else if(event->type()==QEvent::MouseButtonPress || event->type()==QEvent::KeyPress
                || event->type()==QEvent::Wheel) {
            auto widget=qobject_cast<QWidget *>(object);
            if(widget && m_host && (widget==m_host || m_host->isAncestorOf(widget))) reset();
        }
        if(object==m_main && event->type()==QEvent::Resize && m_host
                && m_host->isVisible() && !m_host->isMinimized()
                && !m_host->isMaximized() && !m_host->isFullScreen()) {
            // A live main resize can deliver many Resize events per second.
            // Reuse its bounded observer rather than creating observers/timers
            // (and optional diagnostics) for every native size notification.
            if(m_requested.isEmpty()) m_requested=m_host->saveGeometry();
            if(m_transaction && m_transaction->refresh()) return false;
            if(m_queued) return false;
            m_queued=true;
            m_work.set(m_host);
            // Installing application filters while Qt walks that filter list
            // can redeliver this same event. Capture now, attach after dispatch.
            QTimer::singleShot(0,this,[this] {
                m_queued=false;m_work.clear();
                if(!m_host || !m_host->isVisible() || m_host->isMinimized()
                    || m_host->isMaximized() || m_host->isFullScreen()) return;
                QRect client;
                if(!readRectangle(qtGeometryJson(m_requested).value("clientRect"),client)) return;
                const auto generation=++m_generation;
                QPointer<PaneMainResizeGeometry> guard(this);
                const auto requested=m_requested;
                m_transaction=new PaneGeometryTransaction(m_host,client.size(),[guard,generation] {
                    return guard && guard->m_generation==generation && guard->m_host && guard->m_host->isVisible();
                },[guard,requested] { if(guard) guard->m_restore(requested); });
            });
        }
        return false;
    }
private:
    QPointer<QWidget> m_host,m_main;
    QPointer<PaneGeometryTransaction> m_transaction;
    QByteArray m_requested;
    std::function<void(const QByteArray &)> m_restore;
    quint64 m_generation=0;
    bool m_queued=false;
    LayoutPendingWork m_work;
};
}
