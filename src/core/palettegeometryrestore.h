// SPDX-License-Identifier: MIT
#pragma once
#include <QPointer>
#include <QApplication>
#include <QTimer>
#include <QWidget>
#include <functional>
#include "windowinteraction.h"
namespace Unbound {
// A native palette can lower its initial minimum through several nested layout
// requests, including changes after the first successful resize. Retain the
// requested size through that bounded transaction, without polling or overriding
// a genuine control minimum. The observer expires and never survives user input.
class PaletteGeometryRestore final : public QObject {
public:
    PaletteGeometryRestore(QWidget *host,QSize requested,std::function<bool()> valid,
        std::function<void()> restore)
        : QObject(host),m_host(host),m_requested(requested),m_valid(std::move(valid)),m_restore(std::move(restore)) {
        qApp->installEventFilter(this);
        QTimer::singleShot(1000,this,[this] { finish(); });
        queue();
    }
protected:
    bool eventFilter(QObject *object,QEvent *event) override {
        if(object==m_host && (event->type()==WindowInteractionEvent::eventType() || event->type()==QEvent::Hide)) finish();
        else if(object==m_host && event->type()==QEvent::LayoutRequest) queue();
        else if(event->type()==QEvent::MouseButtonPress || event->type()==QEvent::KeyPress) {
            auto widget=qobject_cast<QWidget *>(object);
            if(m_host && widget && (widget==m_host || m_host->isAncestorOf(widget))) finish();
        }
        return false;
    }
private:
    void finish() {
        if(m_done) return;
        m_done=true;
        if(qApp) qApp->removeEventFilter(this);
        deleteLater();
    }
    void queue() {
        if(m_done || m_queued) return;
        m_queued=true;
        QTimer::singleShot(0,this,[this] {
            m_queued=false;
            if(m_done) return;
            if(!m_host || !m_valid()) { finish(); return; }
            const auto minimum=m_host->minimumSize().expandedTo(m_host->minimumSizeHint());
            if(minimum.width()>m_requested.width() || minimum.height()>m_requested.height()) return;
            if(m_host->size()!=m_requested) m_restore();
        });
    }
    QPointer<QWidget> m_host;
    QSize m_requested;
    std::function<bool()> m_valid;
    std::function<void()> m_restore;
    bool m_done=false,m_queued=false;
};
}
