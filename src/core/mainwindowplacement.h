// SPDX-License-Identifier: MIT
#ifndef UNBOUND_MAINWINDOWPLACEMENT_H
#define UNBOUND_MAINWINDOWPLACEMENT_H
#include "panesettings.h"
#include "layouttracediagnostics.h"
#include <QApplication>
#include <QStackedWidget>
#include <QPointer>
#include <QTimer>
#include <QScreen>
#include <QJsonArray>
#include <QWindowStateChangeEvent>
#include <functional>
#include "windowplacement.h"
#include "panegeometryrestore.h"
#include "windowinteraction.h"
namespace Unbound {
class MainWindowPlacement : public QObject
{
public:
    explicit MainWindowPlacement(PaneSettings &settings, QObject *parent = nullptr)
        : QObject(parent), m_settings(settings)
    {
        m_save.setSingleShot(true); m_save.setInterval(750);
        connect(&m_save,&QTimer::timeout,this,[this] { flush(); });
    }
    ~MainWindowPlacement() override { flush(); }
    std::function<void(const QString &)> error;
    void attach(QWidget *window)
    {
        if (m_window == window && m_stack) return;
        flush();
        for (auto marker:m_dualScreenMarkers) if (marker) marker->removeEventFilter(this);
        m_dualScreenMarkers.clear();
        if (m_window) m_window->removeEventFilter(this);
        if (m_stack) disconnect(m_stack,nullptr,this,nullptr);
        m_window=window; m_stack=nullptr; m_page.clear(); m_last.clear();
        cancelNormalRestore();
        if (!window) return;
        m_stack=window->findChild<QStackedWidget *>(QStringLiteral("widgetStack_Panel"));
        window->installEventFilter(this);
        for (auto child:window->findChildren<QWidget *>()) observeWidget(child);
        if (m_stack) connect(m_stack,&QStackedWidget::currentChanged,this,[this] {
            // Save the last outgoing placement before the incoming page's
            // layout constraints can resize the main window.
            cancelNormalRestore(); flush(); scheduleRefresh();
        });
        refresh();
    }
    void observeWidget(QWidget *widget)
    {
        if (!m_window || !m_window->isAncestorOf(widget)) return;
        const auto name=widget->objectName();
        if (!name.contains(QLatin1String("DualScreenMode")) && !name.endsWith(QLatin1String("InDSM"))) return;
        for (auto marker:m_dualScreenMarkers) if (marker==widget) return;
        m_dualScreenMarkers.append(widget);
        widget->installEventFilter(this);
        scheduleRefresh();
    }
    static QRect reachable(QRect geometry, const QList<QRect> &screens)
    {
        return reachableWindowGeometry(geometry,screens);
    }
    void refresh()
    {
        if (!m_window || !m_stack || m_applying) return;
        if (!eligible()) {
            // Do not overwrite normal page placements with special-mode sizes.
            cancelNormalRestore(); flush(); m_page.clear(); m_last.clear(); return;
        }
        auto page=m_stack->currentWidget();
        const auto name=page ? page->objectName() : QString();
        if (!name.startsWith(QLatin1String("WStackPage_")) || !page->isVisible()) return;
        if (name == m_page) { capture(); return; }
        flush(); m_save.stop(); m_page=name; m_last.clear();
        cancelNormalRestore();
        const auto saved=m_settings.geometry(key());
        m_applying=true;
        if (!saved.isEmpty()) {
            auto document=QJsonDocument::fromJson(saved).object();
            const bool envelope=document.value("format").toString()==QLatin1String("unbound-main-placement-1");
            if (!envelope) document=qtGeometryJson(saved);
            const bool nativeRestored=Platform::restoreWindowPlacement(m_window, document, screenSignature());
            if (!nativeRestored) m_window->restoreGeometry(envelope
                ? QByteArray::fromBase64(document.value("qt").toString().toLatin1()) : saved);
            // Qt restores maximized state as well. Clamp the normal placement
            // only; fullscreen geometry is never saved by this controller.
            if (!nativeRestored && !m_window->isMaximized()) {
                QList<QRect> screens;
                for (auto screen : QGuiApplication::screens()) screens.append(screen->availableGeometry());
                const QRect frame=m_window->frameGeometry();
                const QRect corrected=reachable(frame,screens);
                if (frame != corrected) {
                    const auto client=m_window->geometry();
                    m_window->resize(qMax(1,client.width()+corrected.width()-frame.width()),
                        qMax(1,client.height()+corrected.height()-frame.height()));
                    m_window->move(corrected.topLeft());
                }
            }
        }
        m_applying=false;
        // Native restoration may initially enforce a minimum calculated with
        // the outgoing maximized frame. Retry the same placement once after
        // the normal frame exists; never compensate with a pixel offset.
        if(!saved.isEmpty()) {
            auto request=QJsonDocument::fromJson(saved).object();
            if(request.isEmpty())request=qtGeometryJson(saved);
            if(!request.value("maximized").toBool())queueNormalRestore(saved);
        }
        capture();
    }
    void flush()
    {
        m_save.stop();
        if (m_page.isEmpty() || m_last.isEmpty()) return;
        const auto message=m_settings.save(key(),m_last);
        if (!message.isEmpty() && error) error(message);
    }
    void reloadCurrent() {
        LayoutTraceSpan trace("main_placement_reload");
        // The selected layout has already replaced the saved placement. Drop
        // the outgoing cache before entering the normal page restoration path.
        cancelNormalRestore(); m_save.stop(); m_page.clear(); m_last.clear(); refresh();
    }
protected:
    bool eventFilter(QObject *object,QEvent *event) override
    {
        if (object!=m_window && (event->type()==QEvent::Show || event->type()==QEvent::Hide)) scheduleRefresh();
        if (object == m_window && !m_applying) {
            if(event->type()==WindowInteractionEvent::eventType()) cancelNormalRestore();
            else if(event->type()==QEvent::MouseButtonPress || event->type()==QEvent::KeyPress) {
                ++m_restoreGeneration; m_restoreWork.clear();
            }
            if(event->type()==QEvent::WindowStateChange) {
                const auto old=static_cast<QWindowStateChangeEvent *>(event)->oldState();
                if(m_window->isMaximized() && eligible()) m_maximizedPlacement=savePlacement();
                else if((old&Qt::WindowMaximized) && m_window->windowState()==Qt::WindowNoState
                    && !m_maximizedPlacement.isEmpty()) queueNormalRestore(m_maximizedPlacement);
            }
            if (event->type()==QEvent::Move || event->type()==QEvent::Resize) capture();
            if (event->type()==QEvent::WindowStateChange || event->type()==QEvent::Show)
                scheduleRefresh();
        }
        return QObject::eventFilter(object,event);
    }
private:
    void cancelNormalRestore() {
        ++m_restoreGeneration; m_maximizedPlacement.clear(); m_restoreWork.clear();
    }
    void queueNormalRestore(const QByteArray &saved) {
        auto document=QJsonDocument::fromJson(saved).object();
        if(document.value("format").toString()!=QLatin1String("unbound-main-placement-1"))
            document=qtGeometryJson(saved);
        if(document.isEmpty())return;
        if(document.value("maximized").toBool() && document.contains("normalClientRect"))
            document.insert("clientRect",document.value("normalClientRect"));
        document.insert("maximized",false);
        const auto generation=++m_restoreGeneration;
        const auto page=m_page;
        m_restoreWork.set("main_normal_placement",m_window);
        QTimer::singleShot(0,this,[this,document,generation,page] {
            if(generation!=m_restoreGeneration)return;
            m_restoreWork.clear();
            if(!eligible() || m_window->windowState()!=Qt::WindowNoState || page!=m_page
                || !m_stack || !m_stack->currentWidget() || m_stack->currentWidget()->objectName()!=page)return;
            m_applying=true;
            // The native backend owns frame coordinates and minimum handling.
            // Unsupported/native-desktop mismatches keep the earlier fallback.
            Platform::restoreWindowPlacement(m_window,document,screenSignature());
            m_applying=false; capture();
        });
    }
    static QJsonArray screenSignature()
    {
        QJsonArray result;
        for (auto screen : QGuiApplication::screens()) {
            const auto rect=screen->availableGeometry();
            result.append(QJsonArray{screen->name(),rect.x(),rect.y(),rect.width(),rect.height(),screen->devicePixelRatio()});
        }
        return result;
    }
    QByteArray savePlacement() const
    {
        return Platform::saveWindowPlacement(m_window, screenSignature());
    }
    QString key() const { return QStringLiteral("MainWindow/")+m_page; }
    bool eligible() const
    {
        if (!m_window || !m_window->isVisible() || m_window->isMinimized()
            || m_window->isFullScreen() || QApplication::activeModalWidget()) return false;
        if (!Platform::windowPlacementEligible(m_window)) return false;
        for (const auto &child : m_dualScreenMarkers)
            if (child && child->isVisible()) return false;
        return true;
    }
    void capture()
    {
        if (m_applying || m_page.isEmpty() || !eligible() || !m_stack || !m_stack->currentWidget()
            || m_stack->currentWidget()->objectName()!=m_page) return;
        const auto placement=savePlacement();
        if(m_window->isMaximized()) {
            const auto native=QJsonDocument::fromJson(placement).object();
            if(native.isEmpty() || native.value("maximized").toBool()) m_maximizedPlacement=placement;
        }
        if (placement != m_last) { m_last=placement; m_save.start(); }
    }
    void scheduleRefresh()
    {
        if (m_pending) return;
        m_pending=true;
        m_work.set("main_placement",m_window);
        QTimer::singleShot(0,this,[this] { m_pending=false;m_work.clear();refresh(); });
    }
    PaneSettings &m_settings;
    QPointer<QWidget> m_window;
    QPointer<QStackedWidget> m_stack;
    QList<QPointer<QWidget>> m_dualScreenMarkers;
    QString m_page;
    QByteArray m_last;
    QByteArray m_maximizedPlacement;
    quint64 m_restoreGeneration=0;
    QTimer m_save;
    bool m_applying=false, m_pending=false;
    LayoutTraceWork m_work;
    LayoutTraceWork m_restoreWork;
};
}
#endif
