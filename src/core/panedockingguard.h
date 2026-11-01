// SPDX-License-Identifier: MIT
#pragma once
#include "layoutreadiness.h"
#include <QPointer>
#include <QEvent>
#include <QTimer>
#include <QWidget>
#include <functional>

namespace Unbound {
// Bounded event-driven settlement after docking, never a polling loop.
// Page return starts a new settling interval; intentional native hiding is valid.
class PaneDockingGuard final : public QObject {
public:
    explicit PaneDockingGuard(QObject *owner) : QObject(owner) {
        m_timer.setSingleShot(true); m_timer.setInterval(100);
        m_deadline.setSingleShot(true); m_deadline.setInterval(1000);
        connect(&m_timer,&QTimer::timeout,this,[this] { check(false); });
        connect(&m_deadline,&QTimer::timeout,this,[this] { check(true); });
    }
    ~PaneDockingGuard() override { cancel(); }
    void cancel() {
        m_timer.stop();
        m_deadline.stop();
        m_work.clear();
        for(auto widget:m_ancestors) if(widget) widget->removeEventFilter(this);
        m_ancestors.clear();
        m_page=nullptr; m_content=nullptr; m_finished={};
    }
    void watch(QWidget *content,QWidget *page,std::function<void(bool)> finished) {
        cancel(); m_content=content; m_page=page; m_finished=std::move(finished);

        observeAncestors();
        if(m_page && m_page->isVisible()) beginSettlement();
    }
    static bool collapsed(QWidget *content,QWidget *page) {
        if(!content || !page || !page->isAncestorOf(content) || !content->isVisibleTo(page)) return false;
        QRect area=content->rect();
        for(auto widget=content; widget && widget!=page;) {
            area.translate(widget->pos()); widget=widget->parentWidget();
            if(!widget) return false;
            area=area.intersected(widget->rect());
        }
        return area.width()<=1 || area.height()<=1;
    }
protected:
    bool eventFilter(QObject *object,QEvent *event) override {
        if(object==m_page) {
            if(event->type()==QEvent::Hide) {m_timer.stop();m_deadline.stop();m_work.clear();}
            else if(event->type()==QEvent::Show) beginSettlement();
        }
        if(event->type()==QEvent::ParentChange) observeAncestors();
        if(m_deadline.isActive() && (event->type()==QEvent::Resize || event->type()==QEvent::Move
            || event->type()==QEvent::LayoutRequest || event->type()==QEvent::Show
            || event->type()==QEvent::Hide || event->type()==QEvent::ParentChange)) m_timer.start();
        return false;
    }
private:
    void observeAncestors() {
        for(auto widget:m_ancestors) if(widget) widget->removeEventFilter(this);
        m_ancestors.clear();
        for(auto widget=m_content.data();widget;widget=widget->parentWidget()) {
            m_ancestors.append(widget); widget->installEventFilter(this);
            if(widget==m_page) break;
        }
        if(m_page && !m_ancestors.contains(m_page)) {
            m_ancestors.append(m_page); m_page->installEventFilter(this);
        }
    }
    void beginSettlement() {
        m_work.set(m_page); m_deadline.start(); m_timer.start();
    }
    void check(bool deadline) {
        if(!m_page || !m_content) { cancel(); return; }
        if(!m_page->isVisible()) return;
        const bool failed=collapsed(m_content,m_page);
        // A quiet but collapsed intermediate allocation is not yet failure.
        // Keep observing until allocation recovers or the absolute cap expires.
        if(failed && !deadline) return;

        auto finished=std::move(m_finished); cancel();
        if(finished) finished(failed);
    }
    QPointer<QWidget> m_content,m_page;
    QList<QPointer<QWidget>> m_ancestors;
    QTimer m_timer,m_deadline;
    LayoutPendingWork m_work;
    std::function<void(bool)> m_finished;
};
inline QString dockingFailureMessage(const QString &pane) {
    QString message=QStringLiteral("Docking unavailable\nDaVinci Resolve couldn't make room for this panel. It remains floating.");
    if(pane==QLatin1String("frameEditPageMediaPool"))
        message+=QStringLiteral("\n\nTry disabling \"Use optimized UI layouts for vertical video\" in DaVinci Resolve > Preferences > User > UI Settings, then dock again.");
    else message+=QStringLiteral("\n\nTry making more room in DaVinci Resolve's main window, then dock again.");
    return message;
}
}
