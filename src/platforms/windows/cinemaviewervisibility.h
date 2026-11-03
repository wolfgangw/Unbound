// SPDX-License-Identifier: MIT
#pragma once
#include <QApplication>
#include <QPointer>
#include <QTimer>
#include <QWidget>
#include <qt_windows.h>
#include <functional>

namespace Unbound {
// Windows Cinema Viewer hides the selected player's original top-level window.
// For a floating player that is our host, leaving UiMainWindow's application
// shortcuts active alongside Cinema's copies. Hide the main window as native
// docked Cinema does, then restore only the visibility that we changed.
class CinemaViewerVisibility : public QObject
{
public:
    explicit CinemaViewerVisibility(std::function<void(QWidget *)> activate=[](QWidget *w) { w->activateWindow(); })
        : m_activate(std::move(activate)) { qApp->installEventFilter(this); }
    ~CinemaViewerVisibility() override { cancelReturn(); restore(); }
    void setWindow(QWidget *main) {
        if(m_main==main) return;
        ++m_generation;
        cancelReturn();
        restore();
        m_main=main;
    }
protected:
    bool eventFilter(QObject *object,QEvent *event) override {
        if(m_returnHost) {
            const auto type=event->type();
            if(type==QEvent::MouseButtonPress || type==QEvent::NonClientAreaMouseButtonPress
                || type==QEvent::KeyPress || type==QEvent::Wheel || type==QEvent::ApplicationDeactivate)
                cancelReturn();
            else if(type==QEvent::WindowActivate) {
                auto active=qobject_cast<QWidget *>(object);
                if(active==m_main && QApplication::activeWindow()==m_main) {
                    const auto generation=m_returnGeneration;
                    QTimer::singleShot(0,this,[this,generation] { finishReturn(generation); });
                } else if(active && active->isWindow() && active!=m_returnHost && active!=m_main) cancelReturn();
            }
        }
        if(event->type()!=QEvent::Show && event->type()!=QEvent::Hide) return false;
        auto window=qobject_cast<QWidget *>(object);
        if(!window || !window->isWindow() || !window->inherits("UiCinemaViewerDialog")) return false;
        if(event->type()==QEvent::Show) {
            cancelReturn();
            const auto generation=++m_generation;
            QPointer<QWidget> cinema=window;
            QTimer::singleShot(0,this,[this,cinema,generation] {
                if(generation!=m_generation || !cinema || !cinema->isVisible()
                    || !m_main || !m_main->isVisible() || m_hiddenMain || QCoreApplication::closingDown()) return;
                m_player=floatingViewer(cinema,"EditTimelinePlayer","widgetEditTimelinePlayer");
                if(!m_player) m_player=floatingViewer(cinema,"EditSourcePlayer","widgetEditSourcePlayer");
                if(!m_player) return;
                m_cinema=cinema;
                m_hiddenMain=true;
                m_destroyed=connect(cinema,&QObject::destroyed,this,[this] { queueRestore(); });
                m_main->hide();
            });
        } else if(window==m_cinema) queueRestore();
        return false;
    }
private:
    QWidget *floatingViewer(QWidget *cinema,const char *viewerName,const char *playerName) const {
        if(!cinema->findChild<QWidget *>(QLatin1String(viewerName))) return nullptr;
        auto player=m_main->findChild<QWidget *>(QLatin1String(playerName));
        return player && player->window()->property("unboundFloatingWindow").toBool() ? player : nullptr;
    }
    void queueRestore() {
        const auto generation=++m_generation;
        // Resolve first puts the image widget back into its original player.
        QTimer::singleShot(0,this,[this,generation] {
            if(generation==m_generation) restore(true);
        });
    }
    void restore(bool preserveReturn=false) {
        disconnect(m_destroyed);
        m_cinema=nullptr;
        const bool hidden=m_hiddenMain;
        m_hiddenMain=false;
        if(!hidden || !m_main || QCoreApplication::closingDown() || m_main->isVisible()) return;
        // Resolve has already reactivated the originating host. Showing main
        // can subsequently activate it again and select its docked player.
        // Preserve that native return choice, not a guessed transport target.
        auto active=QApplication::activeWindow();
        if(preserveReturn && m_player && active==m_player->window() && active!=m_main
            && active->isVisible() && !active->isMinimized()) {
            m_returnHost=active;
            auto focus=QApplication::focusWidget();
            m_returnFocus=focus && focus->window()==active ? focus : m_player.data();
            const auto generation=++m_returnGeneration;
            QTimer::singleShot(1000,this,[this,generation] {
                if(generation==m_returnGeneration) cancelReturn();
            });
        }
        const bool noActivate=m_main->testAttribute(Qt::WA_ShowWithoutActivating);
        m_main->setAttribute(Qt::WA_ShowWithoutActivating);
        m_main->show();
        m_main->setAttribute(Qt::WA_ShowWithoutActivating,noActivate);
    }
    void cancelReturn() { ++m_returnGeneration; m_returnHost=nullptr; m_returnFocus=nullptr; }
    void finishReturn(quint64 generation) {
        if(generation!=m_returnGeneration || !m_returnHost) return;
        const auto host=m_returnHost,focus=m_returnFocus;
        cancelReturn();
        if(!m_main || !m_main->isVisible() || !host->isVisible() || host->isMinimized()
            || QApplication::activeWindow()!=m_main || QApplication::activeModalWidget()
            || QApplication::activePopupWidget() || QCoreApplication::closingDown()) return;
        if(QGuiApplication::platformName()==QLatin1String("windows")
            && GetForegroundWindow()!=reinterpret_cast<HWND>(m_main->effectiveWinId())) return;
        m_activate(host);
        if(focus && focus->isVisible() && focus->window()==host) focus->setFocus(Qt::ActiveWindowFocusReason);
    }
    QPointer<QWidget> m_main,m_cinema,m_player,m_returnHost,m_returnFocus;
    std::function<void(QWidget *)> m_activate;
    QMetaObject::Connection m_destroyed;
    quint64 m_generation=0;
    quint64 m_returnGeneration=0;
    bool m_hiddenMain=false;
};
}
