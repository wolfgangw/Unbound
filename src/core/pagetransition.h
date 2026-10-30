// SPDX-License-Identifier: MIT
#pragma once
#include "layouttransition.h"
#include <QStackedWidget>

namespace Unbound {
// Native page handoff. Observe native stack events without consuming input
// or changing its selection. Presentation and readiness reuse layout recall.
class PageTransition final : public QObject {
public:
    using Factory=std::function<std::unique_ptr<LayoutTransitionPresentation>()>;
    std::function<bool(const QString &)> hasFloating;
    std::function<void()> arrived;
    Factory presentation=[] {return makeLayoutTransitionPresentation();};
    PageTransition() {
#ifdef UNBOUND_LAYOUT_TRANSITIONS
        qApp->installEventFilter(this);
#endif
    }
    ~PageTransition() override {cancel("page_observer_destroyed");}
    void attach(QWidget *window) {
#ifdef UNBOUND_LAYOUT_TRANSITIONS
        auto pages=window?window->findChild<QStackedWidget *>("widgetStack_Panel"):nullptr;
        if(main==window && stack==pages)return;
        cancel("page_stack_replaced");
        if(stack)disconnect(stack,nullptr,this,nullptr);
        main=window;stack=pages;departed=false;attempted=false;
        current=stack?stack->currentWidget():nullptr;
        if(stack)connect(stack,&QStackedWidget::currentChanged,this,[this] {
            incoming(stack->currentWidget());
            if(transition && transition->running()) {
                if(arrived)arrived();
            }
        });
#else
        Q_UNUSED(window);
#endif
    }
protected:
    bool eventFilter(QObject *object,QEvent *event) override {
        if(!stack || !main)return false;
        const auto type=event->type();
        if(type!=QEvent::Show && type!=QEvent::Hide)return false;
        if(object==main && type==QEvent::Hide) {cancel("main_hidden");departed=false;return false;}
        auto page=qobject_cast<QWidget *>(object);
        if(!page || stack->indexOf(page)<0 || !main->isVisible() || main->isMinimized()
            || !stack->isVisible())return false;
        if(type==QEvent::Hide && page==current && page==stack->currentWidget()) {
            // Application filters run before pane-session Hide handlers. Capture
            // outgoing floaters now, while their native windows still exist.
            // Duplicate Hide notifications must not cancel or recapture this cover.
            if(departed)return false;
            cancel("next_page_switch");
            departed=true;attempted=false;
            if(hasFloating && hasFloating(page->objectName())) {
                attempted=true;

                // Do not bind the outgoing page: its Hide is this handoff,
                // not cancellation. Readiness starts only on arrival.
                transition=LayoutTransition::begin(main,QString(),presentation());
            }
        } else if(type==QEvent::Show && page==stack->currentWidget())incoming(page);
        return false;
    }
private:
    void start(QWidget *page) {
#ifdef UNBOUND_LAYOUT_TRANSITIONS
        attempted=true;
        LayoutReadiness::begin(main,page->objectName());

        transition=LayoutTransition::begin(main,page->objectName(),presentation());
        if(!transition) {if(LayoutReadiness::active())LayoutReadiness::active()->finish();}
        if(transition) {
            const auto alive=transition;
            transition->releaseReadiness=LayoutReadiness::observe([alive] {if(alive)alive->ready();},
                [alive] {if(alive)alive->invalidate();});
        }
#else
        Q_UNUSED(page);
#endif
    }
    void incoming(QWidget *page) {
#ifdef UNBOUND_LAYOUT_TRANSITIONS
        if(!main || !main->isVisible() || main->isMinimized() || !stack || !stack->isVisible()) {
            cancel("hidden_page_selection");current=page;departed=false;attempted=false;return;
        }
        if(!page || (page==current && !departed))return;
        // Application filters precede the page's pane-session Show handlers.
        // An outgoing cover survives arrival unchanged. Otherwise capture only
        // when the incoming page has floaters; docked-to-docked stays native.
        if(attempted && transition && transition->running()) {
            LayoutReadiness::begin(main,page->objectName());

            transition->destination(page);
            const auto alive=transition;
            transition->releaseReadiness=LayoutReadiness::observe([alive] {if(alive)alive->ready();},
                [alive] {if(alive)alive->invalidate();});
        }
        if(!attempted && hasFloating && hasFloating(page->objectName()))start(page);
        current=page;departed=false;

#else
        Q_UNUSED(page);
#endif
    }
    void cancel(const QString &reason) {
        if(transition)transition->cancel(reason);
        transition=nullptr;
    }
    QPointer<QWidget> main,current;
    QPointer<QStackedWidget> stack;
    QPointer<LayoutTransition> transition;
    bool departed=false,attempted=false;
};
}
