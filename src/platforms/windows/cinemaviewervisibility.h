// SPDX-License-Identifier: MIT
#pragma once
#include <QApplication>
#include <QPointer>
#include <QTimer>
#include <QWidget>

namespace Unbound {
// Windows Cinema Viewer hides the selected player's original top-level window.
// For a floating player that is our host, leaving UiMainWindow's application
// shortcuts active alongside Cinema's copies. Hide the main window as native
// docked Cinema does, then restore only the visibility that we changed.
class CinemaViewerVisibility : public QObject
{
public:
    CinemaViewerVisibility() { qApp->installEventFilter(this); }
    ~CinemaViewerVisibility() override { restore(); }
    void setWindow(QWidget *main) {
        if(m_main==main) return;
        ++m_generation;
        restore();
        m_main=main;
    }
protected:
    bool eventFilter(QObject *object,QEvent *event) override {
        if(event->type()!=QEvent::Show && event->type()!=QEvent::Hide) return false;
        auto window=qobject_cast<QWidget *>(object);
        if(!window || !window->isWindow() || !window->inherits("UiCinemaViewerDialog")) return false;
        if(event->type()==QEvent::Show) {
            const auto generation=++m_generation;
            QPointer<QWidget> cinema=window;
            QTimer::singleShot(0,this,[this,cinema,generation] {
                if(generation!=m_generation || !cinema || !cinema->isVisible()
                    || !m_main || !m_main->isVisible() || m_hiddenMain || QCoreApplication::closingDown()) return;
                if(!floatingViewer(cinema,"EditTimelinePlayer","widgetEditTimelinePlayer")
                    && !floatingViewer(cinema,"EditSourcePlayer","widgetEditSourcePlayer")) return;
                m_cinema=cinema;
                m_hiddenMain=true;
                m_destroyed=connect(cinema,&QObject::destroyed,this,[this] { queueRestore(); });
                m_main->hide();
            });
        } else if(window==m_cinema) queueRestore();
        return false;
    }
private:
    bool floatingViewer(QWidget *cinema,const char *viewerName,const char *playerName) const {
        if(!cinema->findChild<QWidget *>(QLatin1String(viewerName))) return false;
        auto player=m_main->findChild<QWidget *>(QLatin1String(playerName));
        return player && player->window()->property("unboundFloatingWindow").toBool();
    }
    void queueRestore() {
        const auto generation=++m_generation;
        // Resolve first puts the image widget back into its original player.
        QTimer::singleShot(0,this,[this,generation] {
            if(generation==m_generation) restore();
        });
    }
    void restore() {
        disconnect(m_destroyed);
        m_cinema=nullptr;
        const bool hidden=m_hiddenMain;
        m_hiddenMain=false;
        if(!hidden || !m_main || QCoreApplication::closingDown() || m_main->isVisible()) return;
        const bool noActivate=m_main->testAttribute(Qt::WA_ShowWithoutActivating);
        m_main->setAttribute(Qt::WA_ShowWithoutActivating);
        m_main->show();
        m_main->setAttribute(Qt::WA_ShowWithoutActivating,noActivate);
    }
    QPointer<QWidget> m_main,m_cinema;
    QMetaObject::Connection m_destroyed;
    quint64 m_generation=0;
    bool m_hiddenMain=false;
};
}
