// SPDX-License-Identifier: MIT
#pragma once
#include "layouttransitionpresentation.h"
#include "layouttracediagnostics.h"
#include "layoutrestorepolicy.h"
#include <QApplication>
#include <QDialog>
#include <QPointer>
#include <QTimer>
#include <QWindow>
namespace Unbound {
// Shared presentation lifecycle, independent of capture technology and readiness
// detection. The caller supplies ready()/invalidate() and owns actual layout work.
// A missing presentation backend never blocks or delays the layout operation.
class LayoutTransition final : public QObject {
    QPointer<QWidget> main,page;
    QList<QPointer<QWidget>> interactive;
    bool interactiveInput(QObject *object) const {
        auto widget=qobject_cast<QWidget *>(object);
        for(auto window:interactive)if(window && (object==window->windowHandle()
            || (widget && (widget==window || window->isAncestorOf(widget)))))return true;
        return false;
    }
    std::unique_ptr<LayoutTransitionPresentation> cover;
    QTimer deadline;
    bool done=false;
    static QPointer<LayoutTransition> &active() {static QPointer<LayoutTransition> value;return value;}
    LayoutTransition(QWidget *window,const QString &pageName):QObject(qApp),main(window) {
        page=window->findChild<QWidget *>(pageName);
        deadline.setSingleShot(true);
        connect(&deadline,&QTimer::timeout,this,[this] {cancel("timeout_not_ready");});
        connect(window,&QObject::destroyed,this,[this] {cancel("main_destroyed");});
        // Native presentation owns application-departure detection. Resolve can
        // emit Qt inactive/active during reparenting while retaining OS foreground.
        connect(qApp,&QCoreApplication::aboutToQuit,this,[this] {cancel("shutdown");});
    }
    bool eventFilter(QObject *object,QEvent *event) override {
        if((event->type()==QEvent::MouseButtonPress || event->type()==QEvent::KeyPress
            || event->type()==QEvent::Wheel) && !interactiveInput(object))cancel("user_input");
        else if(event->type()==QEvent::Hide && (object==main || object==page))cancel("page_or_main_hidden");
        else if(event->type()==QEvent::Hide) {
            for(auto window:interactive)if(window && object==window) {cancel("interactive_window_hidden");break;}
        }
        else if(event->type()==QEvent::Show) if(auto dialog=qobject_cast<QDialog *>(object))
            if(dialog->isWindow() && !LayoutRestorePolicy::expectedDialog(page?page->objectName():QString(),dialog))cancel("dialog_shown");
        return false;
    }
public:
    std::function<void(const QString &)> ended;
    bool running() const {return !done;}
    void ready() {
        if(done || !cover)return;
        LayoutTrace::mark("cover_reveal_candidate");
        QPointer<LayoutTransition> alive=this;
        cover->reveal(120,[alive](bool success) {if(alive)alive->cancel(success?"revealed":"presentation_failed");});
    }
    void invalidate() {
        if(done || !cover)return;
        LayoutTrace::mark("cover_candidate_invalidated");cover->hold();
    }
    static QPointer<LayoutTransition> begin(QWidget *window,const QString &page,
        std::unique_ptr<LayoutTransitionPresentation> presentation=makeLayoutTransitionPresentation(),
        const QList<QWidget *> &interactiveWindows={}) {
        if(active())active()->cancel("superseded");
        if(!window || !presentation)return {};
        auto experiment=new LayoutTransition(window,page);active()=experiment;
        for(auto live:interactiveWindows)if(live && live->isVisible()) {
            experiment->interactive.append(live);
            connect(live,&QObject::destroyed,experiment,[experiment] {experiment->cancel("interactive_window_destroyed");});
        }
        experiment->cover=std::move(presentation);
        QPointer<LayoutTransition> alive=experiment;
        LayoutTraceSpan capture("cover_capture");
        if(!experiment->cover->begin(window,[alive] {if(alive)alive->cancel("native_interruption");},interactiveWindows)) {
            experiment->cancel("capture_unavailable");return {};
        }
        LayoutTrace::mark("cover_bitmap_bytes",QString::number(experiment->cover->retainedBytes()));
        LayoutTrace::mark("cover_visible");
        qApp->installEventFilter(experiment);
        experiment->deadline.start(3000);
        return experiment;
    }
    void cancel(const QString &reason) {
        if(done)return;
        done=true;deadline.stop();qApp->removeEventFilter(this);
        LayoutTrace::mark("cover_end",reason);
        // Keep the adapter alive until a native/animation callback has unwound.
        if(cover)cover->cancel();
        if(active()==this)active().clear();
        if(ended)ended(reason);
        deleteLater();
    }
};
// Opt-in prototype readiness adapter. Keep its evidence/limitations separate
// from the reusable presentation API. A future readiness source can call the
// same ready()/invalidate() methods without depending on diagnostics.
class LayoutTransitionRecall {
    QPointer<LayoutTransition> experiment;
    bool succeeded=false;
public:
    LayoutTransitionRecall(QWidget *window,const QString &page,std::function<void(const QString &)> ended={},QWidget *interactive=nullptr,
        std::unique_ptr<LayoutTransitionPresentation> presentation={}) {
#ifdef UNBOUND_LAYOUT_COVER_EXPERIMENT
        if(!presentation)presentation=makeLayoutTransitionPresentation();
        experiment=LayoutTransition::begin(window,page,std::move(presentation),interactive?QList<QWidget *>{interactive}:QList<QWidget *>{});
        if(experiment) {
            experiment->ended=std::move(ended);
            const auto alive=experiment;
            LayoutTrace::presentationProbe([alive] {if(alive)alive->ready();},
                [alive] {if(alive)alive->invalidate();});
        }
#else
        Q_UNUSED(window);Q_UNUSED(page);Q_UNUSED(ended);Q_UNUSED(interactive);Q_UNUSED(presentation);
#endif
    }
    ~LayoutTransitionRecall() {if(experiment && !succeeded)experiment->cancel("recall_failed");}
    void complete() {succeeded=true;}
};
}
