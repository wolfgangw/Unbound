// SPDX-License-Identifier: MIT
#pragma once
#include <QAbstractButton>
#include <QCoreApplication>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QPointer>
#include <QTimer>

namespace Unbound {
// Resolve uses the same arrow for the local two-view layout and the width of
// the main tray. The floating view can keep its own choice on tray changes.
class FloatingHdrZones final : public QObject {
public:
    FloatingHdrZones(QWidget *host,QWidget *slot,QObject *owner) : QObject(owner),m_slot(slot),m_host(host) {
        m_host->installEventFilter(this);
    }
    ~FloatingHdrZones() override {
        if(m_host) m_host->removeEventFilter(this);
        if(m_flat) m_flat->removeEventFilter(this);
        if(m_arrow) {
            m_arrow->removeEventFilter(this);
            disconnect(m_arrow,nullptr,this,nullptr);
            // A pending native tray change already has its own arrangement.
            if(!m_pending || m_arrow->isChecked()==m_floating) setLocalView(m_docked);
        }
        releasePresentation();
    }

    // Called by the existing floating-subtree discovery. Only the HDR arrow
    // is exempt from the ordinary native undock guard.
    bool observe(QWidget *widget) {
        if(widget->objectName()==QLatin1String("frameFlatViewContainer") && widget!=m_flat) {
            if(m_flat) m_flat->removeEventFilter(this);
            m_flat=widget;
            m_flat->installEventFilter(this);
        }
        if(!widget->parentWidget()
            || widget->parentWidget()->objectName()!=QLatin1String("colorWheelTopFrame")) return false;
        if(widget->objectName()==QLatin1String("btnWheelsView"))
            bindSingleView(m_wheelsView,widget,SingleView::Wheels);
        else if(widget->objectName()==QLatin1String("btnGraphsView"))
            bindSingleView(m_graphView,widget,SingleView::Graph);
        if(widget->objectName()!=QLatin1String("btnExpandWidget")) return false;
        auto arrow=qobject_cast<QAbstractButton *>(widget);
        if(!arrow || !arrow->isCheckable()) return false;
        if(arrow!=m_arrow) {
            if(m_arrow) {
                m_arrow->removeEventFilter(this);
                disconnect(m_arrow,nullptr,this,nullptr);
            }
            m_arrow=arrow;
            m_docked=arrow->isChecked();
            if(!m_initialized) m_floating=m_docked;
            m_initialized=true;
            arrow->installEventFilter(this);
            connect(arrow,&QAbstractButton::toggled,this,[this](bool checked) {
                if(m_local) return;
                m_docked=checked;
                nativeViewChanged();
            });
            if(m_docked!=m_floating) nativeViewChanged();
        }
        return true;
    }

protected:
    bool eventFilter(QObject *object,QEvent *event) override {
        // Qt may flush the host's backing store before queued reconciliation.
        // Keep its last complete frame until both native views are in place.
        if(object==m_host && event->type()==QEvent::UpdateRequest && (m_pending || m_local)) {
            m_presentationPending=true;
            return true;
        }
        if(object==m_flat) {
            // Tray selection can reset the arrow with its signals blocked.
            // These explicit visibility events also arrive on a hidden page.
            if(event->type()==QEvent::HideToParent || event->type()==QEvent::ShowToParent)
                nativeViewChanged();
            return false;
        }
        if(object!=m_arrow || !m_slot || m_local) return false;
        switch(event->type()) {
        case QEvent::MouseButtonRelease:
            if(static_cast<QMouseEvent *>(event)->button()==Qt::LeftButton && m_arrow->isDown())
                return deliverInput(event);
            break;
        case QEvent::KeyPress:
        case QEvent::KeyRelease: {
            const auto key=static_cast<QKeyEvent *>(event)->key();
            if(key==Qt::Key_Space || key==Qt::Key_Select || key==Qt::Key_Enter || key==Qt::Key_Return)
                return deliverInput(event);
            break;
        }
        case QEvent::Shortcut:
            return deliverInput(event);
        case QEvent::Timer:
            if(m_arrow->isDown()) return deliverInput(event);
            break;
        default: break;
        }
        return false;
    }

private:
    enum class SingleView { Native,Wheels,Graph };
    void bindSingleView(QPointer<QAbstractButton> &bound,QWidget *widget,SingleView view) {
        auto button=qobject_cast<QAbstractButton *>(widget);
        if(!button || button==bound) return;
        if(bound) disconnect(bound,nullptr,this,nullptr);
        bound=button;
        // Resolve may change the selected tab again after processing the
        // tray switch. A click chooses a tab; toggled alone also fires for
        // that later native reset.
        connect(button,&QAbstractButton::clicked,this,[this,view] {
            if(!m_local) m_singleView=view;
        });
        connect(button,&QAbstractButton::toggled,this,[this](bool checked) {
            if(checked && !m_floating && m_singleView!=SingleView::Native) nativeViewChanged();
        });
    }
    void selectPreferredTab() {
        auto button=m_singleView==SingleView::Wheels ? m_wheelsView : m_graphView;
        if(m_singleView!=SingleView::Native && button && button->isEnabled() && !button->isChecked())
            button->click();
    }
    void nativeViewChanged() {
        if(m_local || m_pending || !m_arrow) return;
        m_pending=true;
        QTimer::singleShot(0,this,[this] { reconcileTrayChange(); });
    }
    void reconcileTrayChange() {
        if(!m_pending) return;
        m_pending=false;
        QPointer<FloatingHdrZones> alive=this;
        if(m_slot && m_arrow && m_arrow->isChecked()!=m_floating) {
            m_docked=m_arrow->isChecked();
            setLocalView(m_floating);
        }
        if(alive && m_slot && m_arrow && !m_floating && m_singleView!=SingleView::Native)
            changeLocally([this] { selectPreferredTab(); });
        if(alive) releasePresentation();
    }
    void setLocalView(bool sideBySide) {
        if(!m_slot || !m_arrow || m_arrow->isChecked()==sideBySide) return;
        changeLocally([&] { m_arrow->setChecked(sideBySide); });
    }
    bool deliverInput(QEvent *event) {
        QPointer<FloatingHdrZones> alive=this;
        QPointer<QAbstractButton> receiver=m_arrow;
        reconcileTrayChange();
        if(!alive || !receiver || receiver!=m_arrow || !m_slot) return true;
        const bool wasOpen=receiver->isChecked();
        changeLocally([&] {
            QCoreApplication::sendEvent(receiver,event);
            if(alive && receiver && receiver==m_arrow && wasOpen && !receiver->isChecked()) {
                m_singleView=SingleView::Wheels;
                selectPreferredTab();
            }
        });
        if(alive && m_arrow==receiver && receiver) m_floating=receiver->isChecked();
        return true;
    }
    template<class Action> void changeLocally(Action action) {
        // Native toggled handlers move the actual wheels and graph. Only the
        // palette's outgoing request to resize the tray must be suppressed.
        QPointer<FloatingHdrZones> alive=this;
        auto slot=m_slot;
        const bool wasBlocked=slot->blockSignals(true);
        m_local=true;
        action();
        if(slot) slot->blockSignals(wasBlocked);
        if(alive) {
            m_local=false;
            releasePresentation();
        }
    }
    void releasePresentation() {
        if(!m_presentationPending) return;
        m_presentationPending=false;
        if(m_host) QCoreApplication::postEvent(m_host,new QEvent(QEvent::UpdateRequest),Qt::LowEventPriority);
    }
    QPointer<QWidget> m_slot,m_flat,m_host;
    QPointer<QAbstractButton> m_arrow,m_wheelsView,m_graphView;
    SingleView m_singleView=SingleView::Native;
    bool m_initialized=false,m_docked=false,m_floating=false;
    bool m_local=false,m_pending=false;
    bool m_presentationPending=false;
};
}
