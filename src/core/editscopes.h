// SPDX-License-Identifier: MIT
#pragma once
#include <QAction>
#include <QScopedValueRollback>
#include "nativescopescontent.h"
#include "panedecoration.h"
#include "panesettings.h"
#include "panegeometryrestore.h"
#include "panemainresizegeometry.h"
#include "nativemenutracking.h"

namespace Unbound {
// Edit has no ordinary single-screen dock slot for Scopes. Native visibility
// commands open directly into this host; returning/closing means native Close.
// The content lease releases synchronously on departure, before Color acquires
// the same renderer. Floating intent and host geometry remain page-local.
class EditScopes final : public QObject {
    // Discovery is application-wide, but page/host lifecycle must run only as
    // object filters, after the page transition's application-level capture.
    class NativeWindowObserver final : public QObject {
        EditScopes *scopes;
    public:
        explicit NativeWindowObserver(EditScopes *owner):QObject(owner),scopes(owner) {
            qApp->installEventFilter(this);
        }
        ~NativeWindowObserver() override {if(qApp)qApp->removeEventFilter(this);}
        bool eventFilter(QObject *object,QEvent *event) override {
            if(!scopes->m_stopping && !scopes->m_mutating
                && (event->type()==QEvent::Show || event->type()==QEvent::ParentChange)
                && object->objectName()==key() && scopes->onPage())scopes->queue();
            return false;
        }
    };
public:
    static QString key() {return QStringLiteral("UiWaveformWindow");}
    std::function<void(QWidget *,const QPoint &)> contextMenu;
    std::function<void(const QString &)> report;
    EditScopes(QWidget *main,QWidget *page,PaneSettings &settings,QObject *owner)
        : QObject(owner),m_main(main),m_page(page),m_settings(settings),m_content(this) {
        setObjectName("UnboundEditScopes");
        m_enabled=settings.floating(key());
        page->installEventFilter(this);
        new NativeWindowObserver(this);
        m_save.setSingleShot(true);m_save.setInterval(750);
        connect(&m_save,&QTimer::timeout,this,[this] {flushGeometry();});
        m_content.dockRequested=[this] {close();};
        refresh();
    }
    ~EditScopes() override {
        m_stopping=true;flushGeometry();release();
        if(m_page)m_page->removeEventFilter(this);
        delete m_host.data();
    }
    bool active() const {return m_enabled;}
    QWidget *host() const {return m_host;}
    void flushGeometry() {
        m_save.stop();
        if(m_host && !m_host->isMinimized()) {
            if(m_host->interactiveDragActive()){m_save.start();return;}
            check(m_settings.save(key(),m_host->saveGeometry()));
        }
    }
    void close() {
        if(m_stopping || m_mutating)return;
        m_activateRequested=false;
        flushGeometry();
        if(!release())return;
        m_enabled=false;check(m_settings.saveFloating(key(),false));
        if(m_host){m_host->hide();m_host->deleteLater();m_host=nullptr;m_mainGeometry=nullptr;}
    }
    void prepareLayout(const QJsonObject &panes) {
        if(!panes.value(key()).toObject().value("floating").toBool())close();
    }
    void restoreLayout() {
        m_activateRequested=false;
        m_save.stop();m_enabled=m_settings.floating(key());
        if(m_host) {
            m_host->setProperty("unboundKeepAtop",m_settings.keepAtop(key()));
            applyGeometry(m_settings.geometry(key()));
        }
        queue();
    }
    void refresh() {
        if(!m_main || !m_page || m_stopping)return;
        auto action=m_main->findChild<QAction *>(PaneCatalogue::find(key())->nativeVisibilityAction);
        if(action && action!=m_action) {
            disconnect(m_actionConnection);m_action=action;
            m_actionConnection=connect(action,&QAction::triggered,this,[this](bool on) {
                if(m_mutating || !onPage())return;
                if(!on){close();return;}
                m_activateRequested=true;
                m_enabled=true;check(m_settings.saveFloating(key(),true));queue();
            });
        }
        if(!onPage() || m_mutating || QApplication::activeModalWidget())return;
        auto dialog=m_main->findChild<QWidget *>(key());
        // Startup/native 1/2/4-up commands can show the window without emitting
        // the On action. Adopt a genuinely visible native window on this page.
        if(dialog && dialog->isWindow() && dialog->isVisible()) {
            m_enabled=true;check(m_settings.saveFloating(key(),true));
        }
        if(m_enabled && !m_embedded)queue();
    }
protected:
    bool eventFilter(QObject *object,QEvent *event) override {
        if(m_stopping)return false;
        if(object==m_page) {
            if(event->type()==QEvent::Hide) {
                m_activateRequested=false;
                flushGeometry();
                if(m_main && m_main->isVisible() && !m_main->isMinimized())release();
                if(m_host)m_host->hide();
            } else if(event->type()==QEvent::Show)queue();
        }
        if(object==m_host) {
            if(event->type()==QEvent::Close){close();event->ignore();return true;}
            if(!m_mutating && (event->type()==QEvent::Move || event->type()==QEvent::Resize
                || event->type()==QEvent::WindowStateChange))m_save.start();
        }
        return false;
    }
private:
    bool onPage() const {return m_page && m_page->isVisible() && m_main && m_main->isVisible();}
    void check(const QString &error) {if(!error.isEmpty() && report)report(error);}
    void queue() {
        if(m_queued || m_stopping)return;
        m_queued=true;m_work.set(m_page);
        QTimer::singleShot(0,this,[this] {
            m_queued=false;m_work.clear();
            const bool activate=m_activateRequested;m_activateRequested=false;
            if(!onPage() || m_mutating || m_stopping || QApplication::activeModalWidget())return;
            auto dialog=m_main->findChild<QWidget *>(key());
            if(!m_enabled && dialog && dialog->isWindow() && dialog->isVisible()) {
                m_enabled=true;check(m_settings.saveFloating(key(),true));
            }
            if(m_enabled && !m_embedded)open();
            else if(m_enabled && m_embedded && m_host && !m_host->isVisible()) {
                m_host->show();applyGeometry(m_settings.geometry(key()));
            }
            // Explicit Workspace opening has the same activation semantics as
            // manual undocking. Native initialization during recall stays passive.
            if(activate && m_embedded && m_host && m_host->isVisible()
                && nativeApplicationActive() && !QApplication::activeModalWidget())
                m_host->activateWindow();
        });
    }
    void applyGeometry(const QByteArray &geometry) {
        if(!m_host || geometry.isEmpty())return;
        m_host->restoreGeometry(geometry);
        restoreReachablePaneClientGeometry(m_host,geometry);
        if(m_mainGeometry)m_mainGeometry->reset(geometry);
        QRect client;
        if(readRectangle(qtGeometryJson(geometry).value("clientRect"),client)) {
            const QPointer<PaneFloatingWindow> host=m_host;
            new PaneGeometryTransaction(host,client.size(),[host] {return host && host->isVisible();},
                [host,geometry] {if(host){host->restoreGeometry(geometry);restoreReachablePaneClientGeometry(host,geometry);}});
        }
    }
    void open() {
        QScopedValueRollback<bool> changing(m_mutating,true);
        auto dialog=m_main->findChild<QWidget *>(key());
        if(dialog && !dialog->isWindow())return; // Another page owns the renderer.
        if(!dialog || !dialog->isVisible()) {
            if(!m_action || !m_action->isEnabled())return;
            // Trigger only when native Scopes is actually closed; keep its own
            // renderer initialization and command routing intact.
            m_action->trigger();
            dialog=m_main->findChild<QWidget *>(key());
        }
        if(!dialog || !dialog->isVisible() || !dialog->isWindow())return;
        auto anchor=dialog->findChild<QWidget *>("btnWaveformViewerOptions");
        auto row=anchor ? qobject_cast<QBoxLayout *>(anchor->parentWidget()->layout()) : nullptr;
        auto title=dialog->findChild<QWidget *>("titleFrame");
        if(!row || !title)return; // Leave native Scopes usable if its header changed.
        const bool created=!m_host;
        if(created) {
            m_host=new PaneFloatingWindow;
            m_host->setObjectName("unboundFloating_"+key());m_host->setWindowTitle("Scopes");
            m_host->setProperty("unboundPage",m_page->objectName());
            m_host->setAttribute(Qt::WA_ShowWithoutActivating);
            inheritPaneTaskbarIdentity(m_host,m_main);
            m_host->resize(dialog->size());m_host->installEventFilter(this);
        }
        const auto error=m_content.adopt(m_page,dialog,m_host,m_page,false);
        if(!error.isEmpty()){check(error);return;}
        m_embedded=true;
        m_expand=new ColorExpandGuard(m_host,this);
        m_button=new UndockButton(anchor->parentWidget());
        m_button->setObjectName("resolvePaneUndockButton");m_button->setProperty("unboundPaneName",key());
        m_button->setIcon(QIcon(new UndockIconEngine));m_button->setIconSize(QSize(20,20));
        m_button->setFixedSize(20,20);m_button->setCheckable(true);m_button->setFocusPolicy(Qt::NoFocus);
        m_button->setFloatingState(true);m_button->setToolTip("Click to close; drag to move");m_button->setAccessibleName("Close Scopes");
        m_button->contextMenu=[this](QWidget *button,const QPoint &position){if(contextMenu)contextMenu(button,position);};
        connect(m_button,&QToolButton::clicked,this,[this]{close();});
        row->insertWidget(row->indexOf(anchor),m_button,0,Qt::AlignVCenter);m_button->show();
        m_drag=new PaneHeaderDrag(title,this);
        m_host->setProperty("unboundKeepAtop",m_settings.keepAtop(key()));
        applyPaneDarkCaption(m_host,m_main);
        const auto geometry=m_settings.geometry(key());
        if(geometry.isEmpty())centerNewPane(m_host,panePlacementWorkArea(m_page));
        else applyGeometry(geometry);
        m_host->show();
        if(geometry.isEmpty())centerNewPane(m_host,panePlacementWorkArea(m_page));
        else applyGeometry(geometry);
        ensureReachablePaneGeometry(m_host);
        if(!m_mainGeometry)m_mainGeometry=new PaneMainResizeGeometry(m_host,m_main,[this](const QByteArray &bytes){
            if(m_host){m_host->restoreGeometry(bytes);restoreReachablePaneClientGeometry(m_host,bytes);}
        });
        flushGeometry();
    }
    bool release() {
        if(!m_embedded)return true;
        QScopedValueRollback<bool> changing(m_mutating,true);
        if(!m_content.stop())return false;
        delete m_drag.data();m_drag=nullptr;
        delete m_button.data();m_button=nullptr;
        delete m_expand.data();m_expand=nullptr;
        m_embedded=false;return true;
    }
    QPointer<QWidget> m_main,m_page;
    PaneSettings &m_settings;
    NativeScopesContent m_content;
    QPointer<QAction> m_action;
    QMetaObject::Connection m_actionConnection;
    QPointer<PaneFloatingWindow> m_host;
    QPointer<UndockButton> m_button;
    QPointer<PaneHeaderDrag> m_drag;
    QPointer<ColorExpandGuard> m_expand;
    QPointer<PaneMainResizeGeometry> m_mainGeometry;
    QTimer m_save;
    LayoutPendingWork m_work;
    bool m_enabled=false,m_embedded=false,m_mutating=false,m_queued=false,m_stopping=false;
    bool m_activateRequested=false;
};
}
