// SPDX-License-Identifier: MIT
#pragma once
#include <QApplication>
#include <QHash>
#include <QPointer>
#include <QStackedWidget>
#include <QTimer>
#include "windowinteraction.h"

namespace Unbound {
// Session-only focus memory. Track positive focus changes while the page is
// stable; hiding outgoing windows must not replace the user's last target.
class PageFocus : public QObject {
    struct Target { QPointer<QWidget> window, control; };
public:
    PageFocus() {
        qApp->installEventFilter(this);
        connect(qApp,&QApplication::focusChanged,this,[this](QWidget *,QWidget *now) { remember(now); });
    }
    void attach(QWidget *main) {
        auto stack=main ? main->findChild<QStackedWidget *>("widgetStack_Panel") : nullptr;
        if(m_main==main && m_stack==stack) return;
        cancel();
        if(m_stack) disconnect(m_stack,nullptr,this,nullptr);
        m_main=main; m_stack=stack; m_targets.clear();
        m_page=currentPage();
        if(stack) connect(stack,&QStackedWidget::currentChanged,this,[this] { changed(); });
        remember(QApplication::focusWidget());
    }
protected:
    bool eventFilter(QObject *object,QEvent *event) override {
        if(!m_stack) return false;
        const auto type=event->type();
        if(type==QEvent::ApplicationDeactivate) { m_mousePressed=false; cancel(); return false; }
        if(type==QEvent::MouseButtonPress) m_mousePressed=true;
        if(type==QEvent::MouseButtonRelease) {
            m_mousePressed=false;
            remember(QApplication::focusWidget());
        }
        if(m_pending && (type==QEvent::MouseButtonPress || type==QEvent::NonClientAreaMouseButtonPress
            || type==QEvent::KeyPress || type==QEvent::Wheel || type==WindowInteractionEvent::eventType())) cancel();
        // No widget inspection on paint, playback, timer or pointer movement.
        if(type!=QEvent::Hide && type!=QEvent::WindowActivate) return false;
        auto widget=qobject_cast<QWidget *>(object);
        if(type==QEvent::Hide && widget==m_stack->currentWidget() && m_main && m_main->isVisible()) {
            // Application filters run before the pane sessions hide their hosts.
            m_pending=true;
        }
        if(type==QEvent::WindowActivate && member(widget)) remember(widget->focusWidget() ? widget->focusWidget() : widget);
        return false;
    }
private:
    QString currentPage() const {
        return m_stack && m_stack->currentWidget() ? m_stack->currentWidget()->objectName() : QString();
    }
    bool member(QWidget *window) const {
        return window && (window==m_main || (window->property("unboundFloatingWindow").toBool()
            && window->property("unboundPage").toString()==currentPage()));
    }
    bool onPage(QWidget *control) const {
        if(!control) return false;
        auto window=control->window();
        if(window!=m_main) return member(window);
        for(auto parent=control;parent;parent=parent->parentWidget())
            if(m_stack->indexOf(parent)>=0) return parent==m_stack->currentWidget();
        return true; // Shared main-window controls, outside the page stack.
    }
    void remember(QWidget *control) {
        if(m_pending || m_applying || !m_main || !m_stack || m_page!=currentPage()
            || !m_stack->currentWidget() || !m_stack->currentWidget()->isVisible()
            || !control || !control->isVisible() || !onPage(control)) return;
        auto window=control->window();
        if(!member(window) || QApplication::activeWindow()!=window) return;
        const auto page=m_page;
        const auto generation=m_generation;
        const Target target{window,control==window ? nullptr : control};
        // A click on the native page bar first activates main, then switches
        // pages in the same event. Do not remember that navigation side effect
        // instead of the outgoing floating editor the user was working in.
        QTimer::singleShot(0,this,[this,target,page,generation] {
            if(generation!=m_generation || m_pending || m_mousePressed || QApplication::mouseButtons()!=Qt::NoButton
                || page!=currentPage() || !target.window
                || !target.window->isVisible() || QApplication::activeWindow()!=target.window) return;
            if(target.control && (QApplication::focusWidget()!=target.control || !onPage(target.control))) return;
            m_targets[page]=target;
        });
    }
    void cancel() { ++m_generation; m_pending=false; }
    void changed() {
        ++m_generation; m_pending=true; m_page=currentPage();
        const auto generation=m_generation;
        // Page Show handlers and controller discovery queue their own work.
        // Two event-loop turns let those reveals finish without polling.
        QTimer::singleShot(0,this,[this,generation] {
            if(generation!=m_generation) return;
            QTimer::singleShot(0,this,[this,generation] { restore(generation); });
        });
    }
    void restore(quint64 generation) {
        if(generation!=m_generation) return;
        m_pending=false;
        if(!m_main || !m_stack || m_page!=currentPage() || !m_main->isVisible() || m_main->isMinimized()
            || QCoreApplication::closingDown() || QGuiApplication::applicationState()!=Qt::ApplicationActive
            || QApplication::activeModalWidget() || QApplication::activePopupWidget()
            || !member(QApplication::activeWindow())) return;
        const auto saved=m_targets.value(m_page);
        auto window=saved.window.data();
        const bool valid=member(window) && window->isVisible() && window->isEnabled() && !window->isMinimized();
        if(!valid) window=m_main;
        m_applying=true;
        if(QApplication::activeWindow()!=window) window->activateWindow();
        auto control=saved.control.data();
        if(valid && control && control->window()==window && onPage(control)
            && control->isVisible() && control->isEnabled() && control->focusPolicy()!=Qt::NoFocus)
            control->setFocus(Qt::OtherFocusReason);
        m_applying=false;
        // Keep the remembered target until genuine subsequent focus changes.
        // An unavailable target falls back to Resolve's own main-window focus.
    }
    QPointer<QWidget> m_main;
    QPointer<QStackedWidget> m_stack;
    QHash<QString,Target> m_targets;
    QString m_page;
    quint64 m_generation=0;
    bool m_pending=false,m_applying=false,m_mousePressed=false;
};
}
