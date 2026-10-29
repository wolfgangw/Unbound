// SPDX-License-Identifier: MIT
#pragma once
#include "layouttracediagnostics.h"
#include <QApplication>
#include <QAbstractButton>
#include <QAction>
#include <QDialog>
#include <QLabel>
#include <QRegularExpression>
#include <QPointer>
#include <QTimer>
#include <memory>

namespace Unbound {
// A bounded, GUI-thread transport transaction for native Color mode changes.
// Leases span queued initialization/recall; nested operations share one stop.
// Native buttons retain command routing. No keys, seek, loop change or polling.
class ColorPlaybackPause final : public QObject {
public:
    struct Lease {
        QPointer<ColorPlaybackPause> owner;
        explicit Lease(ColorPlaybackPause *value):owner(value) {}
        ~Lease() {if(owner)owner->release();}
    };
    using Hold=std::shared_ptr<Lease>;
    class Recall {
        Hold &hold;bool succeeded=false;
    public:
        explicit Recall(Hold &value):hold(value) {}
        ~Recall() {if(!succeeded) {cancelActive("recall_failed");hold.reset();}}
        void complete() {succeeded=true;}
    };
private:
    QPointer<QWidget> m_page,m_main,m_player;
    QPointer<QAbstractButton> m_stop,m_forward,m_reverse,m_resume;
    QPointer<QAction> m_faster;
    QPointer<QLabel> m_speedLabel;
    double m_speed=1;
    std::weak_ptr<Lease> m_hold;
    QTimer m_deadline;
    LayoutTraceWork m_work;
    unsigned m_generation=0;
    bool m_cancelled=false,m_ownCommand=false;
    static QPointer<ColorPlaybackPause> &active() {static QPointer<ColorPlaybackPause> value;return value;}
    static Hold unavailable(const char *reason) {
        LayoutTrace::mark("color_playback_pause_unavailable",QLatin1String(reason));return {};
    }
    static QAbstractButton *button(QWidget *root,const char *name) {
        if(!root)return nullptr;
        const auto values=root->findChildren<QAbstractButton *>(QLatin1String(name));
        return values.size()==1 ? values.first() : nullptr;
    }
    bool stopped() const {
        return m_stop && m_forward && m_reverse && m_stop->isChecked()
            && !m_forward->isChecked() && !m_reverse->isChecked();
    }
    double displayedSpeed() const {
        // Resolve retains stale text while hiding this label at normal speed.
        // Use explicit hiding; an ancestor can be hidden during reparenting.
        if(!m_speedLabel || m_speedLabel->isHidden() || m_speedLabel->text().trimmed().isEmpty())return 1;
        static const QRegularExpression pattern(QStringLiteral("^([0-9]+(?:[.,][0-9]+)?)\\s*x$"));
        const auto match=pattern.match(m_speedLabel->text().trimmed());
        return match.hasMatch()?match.captured(1).replace(',', '.').toDouble():0;
    }
    void resumePlayback() {
        m_ownCommand=true;m_resume->click();
        // Native J/L menu actions double speed. The label updates asynchronously
        // and can briefly display old text, so derive a bounded command count
        // from the pre-stop value rather than feeding that stale text back.
        int steps=0;double speed=1;
        while(speed<m_speed && steps<6) {speed*=2;++steps;}
        if(speed==m_speed && (steps==0 || (m_faster && m_faster->isEnabled()))) {
            for(int i=0;i<steps;++i)m_faster->trigger();
        } else if(m_speed!=1) {
            LayoutTrace::mark("color_playback_speed_fallback","1x");
        }
        m_ownCommand=false;
    }
    ColorPlaybackPause(QWidget *page,QWidget *player,QAbstractButton *stop,
        QAbstractButton *forward,QAbstractButton *reverse):QObject(qApp),m_page(page),
        m_main(page->window()),m_player(player),m_stop(stop),m_forward(forward),m_reverse(reverse) {
        m_deadline.setSingleShot(true);
        connect(&m_deadline,&QTimer::timeout,this,[this] {cancel("deadline");});
        connect(qApp,&QCoreApplication::aboutToQuit,this,[this] {cancel("shutdown");});
        connect(page,&QObject::destroyed,this,[this] {cancel("page_destroyed");});
        connect(player,&QObject::destroyed,this,[this] {cancel("player_destroyed");});
        for(auto b:{stop,forward,reverse}) connect(b,&QAbstractButton::clicked,this,[this] {
            if(!m_ownCommand)cancel("new_transport_command");
        });
    }
    Hold lease() {
        ++m_generation;
        if(auto existing=m_hold.lock())return existing;
        auto value=std::make_shared<Lease>(this);m_hold=value;return value;
    }
    void release() {
        const auto generation=++m_generation;
        // Finish queued native layout handlers before resuming; no fixed sleep
        // or nested event loop. A new lease invalidates both continuations.
        QTimer::singleShot(0,this,[this,generation] {
            if(generation!=m_generation)return;
            QTimer::singleShot(0,this,[this,generation] {
                if(generation!=m_generation)return;
                const bool resume=!m_cancelled && m_page && m_page->isVisible()
                    && m_main && m_main->isVisible() && !m_main->isMinimized()
                    && !QCoreApplication::closingDown() && !QApplication::activeModalWidget()
                    && QGuiApplication::applicationState()==Qt::ApplicationActive
                    && stopped() && m_resume && m_resume->isEnabled();
                m_deadline.stop();qApp->removeEventFilter(this);
                if(active()==this)active().clear();
                m_work.clear();
                if(resume) {
                    LayoutTrace::mark("color_playback_resume");
                    resumePlayback();
                } else LayoutTrace::mark("color_playback_resume_skipped");
                deleteLater();
            });
        });
    }
    bool eventFilter(QObject *object,QEvent *event) override {
        switch(event->type()) {
        case QEvent::MouseButtonPress:case QEvent::KeyPress:case QEvent::ShortcutOverride:case QEvent::Wheel:
            cancel("new_input");break;
        case QEvent::Hide:
            if(object==m_page || object==m_main)cancel("page_or_main_hidden");break;
        case QEvent::WindowTitleChange:
            if(object==m_main)cancel("project_changed");break;
        case QEvent::ApplicationDeactivate:
            // Reparenting can cause transient Qt deactivation. Recheck once
            // after that event unwinds, never repeatedly while inactive.
            QTimer::singleShot(0,this,[this] {
                if(QGuiApplication::applicationState()!=Qt::ApplicationActive)cancel("application_inactive");
            });break;
        case QEvent::Show:
            if(auto dialog=qobject_cast<QDialog *>(object))if(dialog->isWindow()
                && dialog->objectName()!=QLatin1String("UiFloatingPaletteWidgetDialog"))cancel("dialog");
            break;
        default:break;
        }
        return false;
    }
public:
    void cancel(const QString &reason) {
        if(m_cancelled)return;
        m_cancelled=true;m_deadline.stop();qApp->removeEventFilter(this);m_work.clear();
        LayoutTrace::mark("color_playback_cancel",reason);
        if(active()==this)active().clear();
        ++m_generation;deleteLater();
    }
    static void cancelActive(const QString &reason) {if(active())active()->cancel(reason);}
    // Optional assistance, never a precondition for changing the layout. An
    // empty lease means keep going without scheduling any automatic resume.
    static Hold acquire(QWidget *page) {
        if(QCoreApplication::closingDown())return {};
        if(!page || page->objectName()!=QLatin1String("WStackPage_Color") || !page->isVisible())return {};
        if(auto transaction=active()) {
            if(transaction->m_page==page && !transaction->m_cancelled && transaction->stopped())
                return transaction->lease();
            transaction->cancel("superseded");
        }
        QWidget *player=page->findChild<QWidget *>("m_pFramePlayer");
        for(auto window:QApplication::topLevelWidgets())
            if(window->objectName()==QLatin1String("unboundFloating_m_pFramePlayer")
                && window->property("unboundFloatingWindow").toBool()) {player=window;break;}
        auto forward=button(player,"btnTransportPlay"),reverse=button(player,"btnTransportReversePlay"),stop=button(player,"btnTransportStop");
        // Unavailable player (including startup/fixtures) has no known playback
        // to pause. Unknown transport disables this workaround, not layout work.
        if(!forward && !reverse && !stop)return {};
        if(!forward || !reverse || !stop)return unavailable("incomplete_transport");
        // Fresh Resolve sessions can leave all three buttons unchecked while
        // idle. Only the directional buttons identify playback; do not require
        // a prior Stop command or create a resume lease for that idle state.
        if(!forward->isChecked() && !reverse->isChecked())return {};
        if(forward->isChecked()==reverse->isChecked())return unavailable("ambiguous_direction");
        if(!stop->isEnabled())return unavailable("stop_disabled");
        auto transaction=new ColorPlaybackPause(page,stop->parentWidget(),stop,forward,reverse);
        transaction->m_resume=reverse->isChecked()?reverse:forward;
        const auto actions=page->window()->findChildren<QAction *>(reverse->isChecked()?"controlPlayReverse":"controlPlayForward");
        if(actions.size()==1)transaction->m_faster=actions.first();
        const auto labels=player->findChildren<QLabel *>("labelPlaySpeed");
        if(labels.size()==1)transaction->m_speedLabel=labels.first();
        transaction->m_speed=transaction->displayedSpeed();
        if(!transaction->m_resume->isEnabled()) {delete transaction;return unavailable("resume_disabled");}
        transaction->m_ownCommand=true;stop->click();transaction->m_ownCommand=false;
        // A checked Stop alone could be the button's own toggle. Require the
        // native handler to clear both directional buttons as well.
        if(!transaction->stopped()) {delete transaction;return unavailable("stop_unconfirmed");}
        active()=transaction;
        transaction->m_work.set("color_playback_pause",page);
        LayoutTrace::mark("color_playback_stop");
        qApp->installEventFilter(transaction);transaction->m_deadline.start(3000);
        return transaction->lease();
    }
};
}
