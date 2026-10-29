// SPDX-License-Identifier: MIT
#pragma once
#include "layouttracediagnostics.h"
#include <QPointer>
#include <QEvent>
#include <QTimer>
#include <QWidget>
#include <functional>

namespace Unbound {
// One bounded check after docking, never a recurring discovery/polling loop.
// Page return starts a new settling interval; intentional native hiding is valid.
class PaneDockingGuard final : public QObject {
public:
    explicit PaneDockingGuard(QObject *owner) : QObject(owner) {
        m_timer.setSingleShot(true); m_timer.setInterval(100);
        connect(&m_timer,&QTimer::timeout,this,[this] { check(); });
    }
    ~PaneDockingGuard() override { cancel(); }
    void cancel() {
        m_timer.stop();
        m_work.clear();
        if(m_page) m_page->removeEventFilter(this);
        m_page=nullptr; m_content=nullptr; m_finished={};
    }
    void watch(QWidget *content,QWidget *page,std::function<void(bool)> finished) {
        cancel(); m_content=content; m_page=page; m_finished=std::move(finished);
        LayoutTrace::mark("docking_check_scheduled",content?content->objectName():QString());
        m_work.set("docking_check",page);
        if(m_page) { m_page->installEventFilter(this); if(m_page->isVisible()) m_timer.start(); }
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
            if(event->type()==QEvent::Hide) {m_timer.stop();m_work.clear();}
            else if(event->type()==QEvent::Show) {m_work.set("docking_check",m_page);m_timer.start();}
        }
        return false;
    }
private:
    void check() {
        if(!m_page || !m_content) { cancel(); return; }
        if(!m_page->isVisible()) return;
        const bool failed=collapsed(m_content,m_page);
        LayoutTrace::mark(failed?"docking_check_failed":"docking_check_passed",m_content->objectName());
        auto finished=std::move(m_finished); cancel();
        if(finished) finished(failed);
    }
    QPointer<QWidget> m_content,m_page;
    QTimer m_timer;
    LayoutTraceWork m_work;
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
