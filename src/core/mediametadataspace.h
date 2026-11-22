// SPDX-License-Identifier: MIT
#pragma once
#include <QAbstractButton>
#include <QEvent>
#include <QCoreApplication>
#include <QLayout>
#include <QPointer>
#include <QScopedValueRollback>
#include <QTimer>
#include <QVector>
#include <utility>
#include "nativetogglestate.h"
#include "windowinteraction.h"

namespace Unbound {
// Media's native selector enables both its sidebar and clip-selection updates.
// Measure the page's own closed allocation, then keep the data source enabled.
class MediaMetadataSpace final : public QObject {
public:
    MediaMetadataSpace(QWidget *slot, QWidget *page, QAbstractButton *button)
        : m_slot(slot), m_page(page), m_button(button) {
        m_page->installEventFilter(this);
        m_main=m_page->window();
        if(m_main!=m_page) m_main->installEventFilter(this);
        connect(button,&QAbstractButton::clicked,this,[this] {
            if(!m_changing) setExpanded(m_reclaimed);
        });
    }
    ~MediaMetadataSpace() override {
        QScopedValueRollback<bool> guard(m_changing,true);
        restoreRegions();
        if(m_button && NativeToggleState::temporary(m_button) && m_button->isChecked()
            && !QCoreApplication::closingDown()) m_button->click();
        m_intent.end();
    }
    bool expanded() const { return !m_reclaimed; }
    bool updating() const { return m_changing; }
    QAbstractButton *button() const { return m_button; }
    void setExpanded(bool expanded) {
        QScopedValueRollback<bool> guard(m_changing,true);
        m_reclaimed=!expanded;
        restoreRegions();
        m_intent.end();
        if(!available()) return;
        if(expanded) {
            if(!m_button->isChecked()) m_button->click();
        } else if(m_resizing) closeForResize();
        else measure();
    }
private:
    struct Region {
        QPointer<QWidget> widget;
        QRect closed, open;
        bool closedHidden, openHidden;
    };
    bool available() const {
        return m_page && m_page->isVisible() && m_slot && m_slot->parentWidget()
            && m_button && m_button->isVisible() && m_button->isEnabled();
    }
    void restoreRegions(bool restoreBounds=true) {
        const auto regions=std::move(m_regions);
        m_regions.clear();
        for(const auto &r:regions) if(auto w=r.widget.data()) {
            w->removeEventFilter(this);
            if(!restoreBounds || w->parentWidget()!=m_parent) continue;
            if(!r.openHidden && w->geometry()==r.closed && r.open!=r.closed) w->setGeometry(r.open);
            if(r.openHidden!=r.closedHidden && w->isHidden()==r.closedHidden) w->setVisible(!r.openHidden);
        }
        if(m_parent && m_parent!=m_page && m_parent!=m_main) m_parent->removeEventFilter(this);
        m_parent=nullptr;
    }
    void closeForResize() {
        // Leave the already reclaimed bounds in place. Resolve can now allocate
        // the page normally for every intermediate size, even during a long hold.
        QScopedValueRollback<bool> guard(m_changing,true);
        restoreRegions(false);
        if(m_button->isChecked()) m_button->click();
        m_intent.end();
    }
    void measure() {
        if(!available()) return;
        QScopedValueRollback<bool> guard(m_changing,true);
        restoreRegions();
        m_intent.end();
        if(m_button->isChecked()) m_button->click();
        if(!available() || m_button->isChecked()) return;
        m_intent.begin(m_button);
        m_parent=m_slot->parentWidget();
        m_parentSize=m_parent->size();
        QPointer<QLayout> layout=m_parent->layout();
        if(layout) layout->activate();
        for(auto w:m_parent->findChildren<QWidget *>(QString(),Qt::FindDirectChildrenOnly))
            if(!w->isWindow()) m_regions.append({w,w->geometry(),{},w->isHidden(),true});
        // Opening the selector for data briefly reinstates the dock allocation.
        // Keep that intermediate layout out of the page's paint on release.
        const QPointer<QWidget> surface=m_parent;
        const bool paint=surface->updatesEnabled();
        if(paint) surface->setUpdatesEnabled(false);
        m_button->click();
        if(layout) layout->activate();
        for(auto &r:m_regions) if(auto w=r.widget.data()) {
            r.open=w->geometry();
            r.openHidden=w->isHidden();
        }
        applyClosed();
        if(paint && surface) surface->setUpdatesEnabled(true);
        for(const auto &r:m_regions) if(r.widget) r.widget->installEventFilter(this);
        if(m_parent && m_parent!=m_page && m_parent!=m_main) m_parent->installEventFilter(this);
    }
    void applyClosed() {
        if(!m_parent) return;
        QScopedValueRollback<bool> guard(m_changing,true);
        QPointer<QLayout> layout=m_parent->layout();
        for(const auto &r:m_regions) if(auto w=r.widget.data()) {
            if(w->parentWidget()!=m_parent) continue;
            if(r.closedHidden!=r.openHidden && w->isHidden()!=r.closedHidden) w->setVisible(!r.closedHidden);
            if(!layout && !r.closedHidden && r.closed!=r.open && w->geometry()!=r.closed) w->setGeometry(r.closed);
        }
        if(layout) layout->activate();
    }
    void queueMeasure() {
        if(m_resizing || m_queued) return;
        m_queued=true;
        QTimer::singleShot(0,this,[this] {
            m_queued=false;
            if(m_resizing || !m_reclaimed || !available()) return;
            // Ignore our own deferred layout notifications and changes to hidden
            // siblings. A native reallocation is measured once per event turn.
            if(m_parent==m_slot->parentWidget() && m_parent->size()==m_parentSize && m_button->isChecked()) {
                bool newLayout=false, alreadyClosed=true;
                for(const auto &r:m_regions) if(auto w=r.widget.data()) {
                    const bool hidden=w->isHidden();
                    const bool managed=m_parent->layout()!=nullptr;
                    const bool closed=hidden==r.closedHidden && (hidden || managed || w->geometry()==r.closed);
                    const bool open=hidden==r.openHidden && (hidden || managed || w->geometry()==r.open);
                    if(w->parentWidget()!=m_parent || (!closed && !open)) { newLayout=true; break; }
                    if(!closed) alreadyClosed=false;
                }
                // If Resolve merely reasserted the last open layout, return the
                // space without toggling Metadata or triggering a new measurement.
                if(!newLayout) { if(!alreadyClosed) applyClosed(); return; }
            }
            measure();
        });
    }
    bool eventFilter(QObject *object,QEvent *event) override {
        if(object==m_main && event->type()==MainWindowResizeEvent::eventType()) {
            m_resizing=static_cast<MainWindowResizeEvent *>(event)->started;
            if(m_reclaimed && available()) {
                if(m_resizing) closeForResize();
                else queueMeasure();
            }
            return false;
        }
        if(m_changing || !m_reclaimed || (object==m_main && object!=m_page && object!=m_parent)) return false;
        if(object==m_page && event->type()==QEvent::Hide) {
            QScopedValueRollback<bool> guard(m_changing,true);
            restoreRegions();
            if(m_button && NativeToggleState::temporary(m_button) && m_button->isChecked()
                && !QCoreApplication::closingDown()) m_button->click();
            m_intent.end();
        } else switch(event->type()) {
        case QEvent::Move: case QEvent::Resize: case QEvent::ShowToParent:
        case QEvent::HideToParent: case QEvent::Show: case QEvent::ParentChange:
            queueMeasure(); break;
        default: break;
        }
        return false;
    }
    QPointer<QWidget> m_slot,m_page,m_parent,m_main;
    QPointer<QAbstractButton> m_button;
    QSize m_parentSize;
    QVector<Region> m_regions;
    NativeToggleState m_intent;
    bool m_reclaimed=false,m_changing=false,m_queued=false,m_resizing=false;
};
}
