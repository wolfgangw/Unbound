// SPDX-License-Identifier: MIT
#pragma once
#include "layoutreadiness.h"
#include "paletterestorationdiagnostics.h"
#include <QPointer>
#include <QApplication>
#include <QTimer>
#include <QWidget>
#include <functional>
#include "windowinteraction.h"
namespace Unbound {
// Native content can lower its initial minimum through several nested layout
// requests, including changes after the first successful resize. Retain the
// requested size through that bounded transaction, without polling or overriding
// a genuine control minimum. The observer expires and never survives user input.
class PaneGeometryTransaction final : public QObject {
public:
    PaneGeometryTransaction(QWidget *host,QSize requested,std::function<bool()> valid,
        std::function<void()> restore)
        : QObject(host),m_host(host),m_requested(requested),m_valid(std::move(valid)),m_restore(std::move(restore)) {
        // Supersede any older request for this host, including queued callbacks.
        for(auto child:host->findChildren<QObject *>("unboundGeometryTransaction",Qt::FindDirectChildrenOnly))
            if(child!=this) static_cast<PaneGeometryTransaction *>(child)->finish("superseded");
        setObjectName("unboundGeometryTransaction");

#ifdef UNBOUND_PALETTE_SIZE_DIAGNOSTICS
        m_diagnostics=new PaletteRestorationDiagnostics(host,requested);
#endif
        qApp->installEventFilter(this);
        m_quiet.setSingleShot(true);
        m_quiet.setInterval(1000);
        connect(&m_quiet,&QTimer::timeout,this,[this] { finish("layout_quiet"); });
        m_quiet.start();
        // Palette creation is only the beginning of a multi-pane restore.
        // Native Lightbox/layout work can arrive after that first second. Keep
        // observing while layout requests continue, with an absolute bound.
        QTimer::singleShot(3000,this,[this] { finish("observer_deadline"); });
        queue();
    }
    bool refresh() {
        if(m_done) return false;
        m_quiet.start(); queue(); return true;
    }
protected:
    bool eventFilter(QObject *object,QEvent *event) override {
        if(object==m_host && (event->type()==WindowInteractionEvent::eventType() || event->type()==QEvent::Hide || event->type()==QEvent::WindowStateChange))
            finish(event->type()==QEvent::Hide?"host_hidden":"geometry_gesture");
        else if(object==m_host && event->type()==QEvent::LayoutRequest) {m_quiet.start();queue();}
        else if(event->type()==QEvent::MouseButtonPress || event->type()==QEvent::KeyPress) {
            auto widget=qobject_cast<QWidget *>(object);
            if(m_host && widget && (widget==m_host || m_host->isAncestorOf(widget))) finish("host_input");
        }
        return false;
    }
private:
    void diagnostic(const char *phase) {
#ifdef UNBOUND_PALETTE_SIZE_DIAGNOSTICS
        if(m_diagnostics)m_diagnostics->record(QString::fromLatin1(phase));
#else
        Q_UNUSED(phase);
#endif
    }
    void finish(const char *reason) {
        if(m_done) return;
        diagnostic(reason);

        m_done=true;
        m_quiet.stop();
        m_work.clear();
        if(qApp) qApp->removeEventFilter(this);
        deleteLater();
    }
    void queue() {
        if(m_done || m_queued) return;
        m_queued=true;
        m_work.set(m_host);
        QTimer::singleShot(0,this,[this] {
            m_queued=false;
            if(m_done) return;
            if(!m_host || !m_valid()) { finish("restore_invalidated"); return; }
            m_work.clear(); // Only queued correction work blocks presentation, never an idle constraint observer.
            const auto minimum=m_host->minimumSize().expandedTo(m_host->minimumSizeHint());
            if(minimum.width()>m_requested.width() || minimum.height()>m_requested.height()) {diagnostic("minimum_blocks_restore");return;}
            if(m_requested.width()>m_host->maximumWidth() || m_requested.height()>m_host->maximumHeight()) return;
            if(m_host->size()!=m_requested) {diagnostic("reapply");m_restore();}
            if(m_host && m_host->size()==m_requested) m_work.clear();
        });
    }
    QPointer<QWidget> m_host;
    QSize m_requested;
    std::function<bool()> m_valid;
    std::function<void()> m_restore;
    bool m_done=false,m_queued=false;
    LayoutPendingWork m_work;
    QTimer m_quiet{this};
#ifdef UNBOUND_PALETTE_SIZE_DIAGNOSTICS
    QPointer<PaletteRestorationDiagnostics> m_diagnostics;
#endif
};
}
