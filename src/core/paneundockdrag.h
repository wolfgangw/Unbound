// SPDX-License-Identifier: MIT
#pragma once
#include "windowintegration.h"
#include "windowinteraction.h"
#include "nativemenutracking.h"
#include <QApplication>
#include <QAbstractButton>
#include <QMouseEvent>
#include <QLayout>
#include <QPointer>
#include <QTimer>
#include <functional>

namespace Unbound {
// Own the gesture independently of the pressed button: native palette expansion
// and Timeline's header transfer can hide, reparent or replace that button.
class PaneUndockDrag final : public QObject {
public:
    using Target = std::function<QWidget *()>;
    static void start(QObject *owner,QWidget *source,const QPoint &offset,
        std::function<void()> undock,Target target) {
        if(current()) current()->finish(true);
        auto gesture=new PaneUndockDrag(owner,source,offset,std::move(target));
        current()=gesture;
        QPointer<PaneUndockDrag> alive=gesture;
        undock();
        if(!alive) return;
        gesture->m_transferring=false;
        gesture->attach();
    }
    static bool ownsHost(QWidget *host) {
        auto gesture=current();
        return host && gesture && !gesture->m_done && !gesture->m_paneName.isEmpty()
            && host->objectName()==QStringLiteral("unboundFloating_")+gesture->m_paneName;
    }
    static void prepareHost(QWidget *host) {
        if(!ownsHost(host)) return;
        auto gesture=current();
        if(gesture->m_preparedHost==host) return;
        gesture->m_preparedHost=host;
        gesture->m_opacity=host->windowOpacity();
        host->setProperty("unboundUndockDragging",true);
        // Show-time native sizing still runs, but the remembered/centered
        // placement must never flash before the button is anchored.
        host->setWindowOpacity(0);
    }
    ~PaneUndockDrag() override { release(true); }
private:
    PaneUndockDrag(QObject *owner,QWidget *source,QPoint offset,Target target)
        : QObject(owner),m_source(source),m_offset(offset),m_target(std::move(target)),
          m_paneName(source->property("unboundPaneName").toString()) {
        qApp->installEventFilter(this);
        // A deadline bounds unavailable/native-deferred targets; discovery is
        // driven by widget events, never a retry timer or an idle scan.
        QTimer::singleShot(1500,this,[this] { if(!m_host) finish(true); });
    }
    bool eventFilter(QObject *object,QEvent *event) override {
        if(m_done) return false;
        if(event->type()==QEvent::KeyPress && static_cast<QKeyEvent *>(event)->key()==Qt::Key_Escape) {
            finish(true); return true;
        }
        // Native palette reparenting can transiently deactivate Qt while
        // Resolve still owns the foreground. Only actual app departure cancels.
        if(event->type()==QEvent::ApplicationDeactivate && !m_transferring
            && !nativeApplicationActive()) { finish(true); return false; }
        if(event->type()==QEvent::MouseButtonPress) { finish(true); return false; }
        if(!m_attaching && m_host && object==m_host && (event->type()==QEvent::Hide || event->type()==QEvent::Close
            || event->type()==QEvent::UngrabMouse)) { finish(true); return false; }
        // Let QWidgetWindow finish native mouse bookkeeping before consuming
        // widget delivery, as in the existing button/header drag handlers.
        if(object->isWidgetType() && event->type()==QEvent::MouseMove) {
            if(m_host) m_host->updateButtonMove();
            else queueAttach();
            return true;
        }
        if(object->isWidgetType() && event->type()==QEvent::MouseButtonRelease
            && static_cast<QMouseEvent *>(event)->button()==Qt::LeftButton) {
            m_released=true;
            m_releasePosition=static_cast<QMouseEvent *>(event)->globalPos();
            if(m_host) finish(false);
            else queueAttach();
            return true;
        }
        if(!m_host && (event->type()==QEvent::Show || event->type()==QEvent::LayoutRequest
            || event->type()==QEvent::ParentChange)) queueAttach();
        return false;
    }
    void queueAttach() {
        if(m_queued || m_done) return;
        m_queued=true;
        QTimer::singleShot(0,this,[this] { m_queued=false; if(!m_transferring) attach(); });
    }
    void attach() {
        if(m_done || m_host) return;
        if(QApplication::activeModalWidget()) { finish(true); return; }
        QPointer<QWidget> button=m_target();
        QPointer<PaneFloatingWindow> host=button ? dynamic_cast<PaneFloatingWindow *>(button->window()) : nullptr;
        if(!host || !host->isVisible() || !button->isVisible()) return;
        m_host=host;
        m_attaching=true;
        host->setProperty("unboundUndockDragging",true);
        // A pull-out gesture always creates a movable normal window, retaining
        // the restored normal size if the saved state was maximized/fullscreen.
        if(host->isMaximized() || host->isFullScreen()) host->showNormal();
        if(m_done || !host || !button) return;
        if(host->layout()) host->layout()->activate();
        WindowInteractionEvent::notify(host);
        if(m_done || !host || !button) return;
        host->move(host->pos()+(m_released ? m_releasePosition : QCursor::pos())-button->mapToGlobal(m_offset));
        // Keep the pulled pane visible without activation's synchronous native
        // accessibility work interrupting pointer movement.
        host->raise();
        reveal();
        m_attaching=false;
        if(m_released || !(QApplication::mouseButtons() & Qt::LeftButton)) { finish(false); return; }
        if(!host->beginButtonMove(false)) { finish(true); return; }
        host->grabMouse();
    }
    void reveal() {
        if(m_preparedHost) m_preparedHost->setWindowOpacity(m_opacity);
        m_preparedHost=nullptr;
    }
    void release(bool cancel) {
        if(current()==this) current()=nullptr;
        if(qApp) qApp->removeEventFilter(this);
        if(auto button=qobject_cast<QAbstractButton *>(m_source.data())) button->setDown(false);
        const auto host=m_host; m_host=nullptr;
        if(host) {
            host->finishButtonMove(cancel);
            if(QWidget::mouseGrabber()==host) host->releaseMouse();
            host->setProperty("unboundUndockDragging",QVariant());
        }
        if(m_preparedHost) m_preparedHost->setProperty("unboundUndockDragging",QVariant());
        reveal();
    }
    void finish(bool cancel) {
        if(m_done) return;
        const auto host=m_host;
        m_done=true; release(cancel); deleteLater();
        if(!cancel && host && host->isVisible() && nativeApplicationActive()
            && !QApplication::activeModalWidget()) host->activateWindow();
    }
    static QPointer<PaneUndockDrag> &current() { static QPointer<PaneUndockDrag> value; return value; }
    QPointer<QWidget> m_source,m_preparedHost;
    QPointer<PaneFloatingWindow> m_host;
    QPoint m_offset,m_releasePosition;
    Target m_target;
    QString m_paneName;
    qreal m_opacity=1;
    bool m_transferring=true,m_released=false,m_queued=false,m_done=false,m_attaching=false;
};
}
