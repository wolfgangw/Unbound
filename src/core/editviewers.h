// SPDX-License-Identifier: MIT
#pragma once
#include <QApplication>
#include <QBoxLayout>
#include <QCloseEvent>
#include <QMessageBox>
#include <QPointer>
#include <QResizeEvent>
#include <QScreen>
#include <QScopedValueRollback>
#include <QTimer>
#include "windowintegration.h"
#include "panedecoration.h"
#include "viewergeometryadapter.h"
#include "panesettings.h"
#include "panedockingguard.h"
#include "panegeometryrestore.h"
#include "panegeometrytransaction.h"
#include "panemainresizegeometry.h"
#include "auditionactivationbridge.h"
#include "layouttracediagnostics.h"
#include "panepagecontext.h"

// Whole native viewer widgets retain their layout slots while floating.
// Share the controller's settings instance so saves cannot overwrite each other.
class EditViewers : public QObject
{
    struct Pane {
        QPointer<QWidget> content, parent, placeholder;
        QPointer<QBoxLayout> originalLayout;
        QPointer<Unbound::PaneFloatingWindow> host;
        QPointer<Unbound::PaneMainResizeGeometry> mainResizeGeometry;
        QPointer<Unbound::AuditionActivationBridge> activationBridge;
        QList<QPointer<Unbound::UndockButton>> buttons;
        int stretch=0;
        Qt::Alignment alignment;
        bool hidden=false;
        QByteArray returningGeometry;
        Unbound::PaneDockingGuard *dockingGuard=nullptr;
    };
public:
    EditViewers(QWidget *main, QWidget *players, QWidget *program, QWidget *source,
        Unbound::PaneSettings &settings, QObject *owner=nullptr,
        std::function<void(QWidget *,const QPoint &)> contextMenu={})
        : QObject(owner ? owner : main), m_players(players), m_main(main), m_settings(settings),m_contextMenu(std::move(contextMenu))
    {
        setObjectName("UnboundEditViewers");
        m_program.content=program; m_source.content=source;
        m_program.dockingGuard=new Unbound::PaneDockingGuard(this);
        m_source.dockingGuard=new Unbound::PaneDockingGuard(this);
        m_page=players;
        while (m_page && m_page->objectName()!="WStackPage_Conform") m_page=m_page->parentWidget();
        if(!m_page && Unbound::auxiliaryPanePage(players)==&Unbound::PaneCatalogue::page(Unbound::PaneCatalogue::Page::Edit))
            m_page=main->findChild<QWidget *>("WStackPage_Conform");
        if (m_page) m_page->installEventFilter(this);
        source->installEventFilter(this);
        program->installEventFilter(this);
        m_geometry=new ViewerGeometryAdapter(players,m_page,this);
        m_saveTimer.setSingleShot(true); m_saveTimer.setInterval(750);
        connect(&m_saveTimer,&QTimer::timeout,this,[this] { rememberAll(); });
        m_restoreProgram=m_settings.floating(program->objectName());
        m_restoreSource=m_settings.floating(source->objectName());
        installButtons(m_program);
        installButtons(m_source);
        queueReclaim();
    }
    ~EditViewers() override
    {
        m_stopping=true; rememberAll(); restoreArea(); dock(m_source,false); dock(m_program,false);
        m_source.dockingGuard->cancel(); m_program.dockingGuard->cancel();
        if(m_page) m_page->removeEventFilter(this);
        qDeleteAll(m_drags);
        for (auto pane:{&m_program,&m_source}) for(auto b:pane->buttons) delete b.data();
    }
    void toggleProgram() { toggle(m_program); }
    void toggleSource() { toggle(m_source); }
    void dockBoth() { restoreArea(); dock(m_source); dock(m_program); }
    bool hasFloating() const { return m_program.host || m_source.host; }
    std::function<void()> dockingWarningChanged;
    void flushGeometry() { rememberAll(); }
    void prepareLayout(const QJsonObject &panes) {
        for(auto pane:{&m_source,&m_program}) if(pane->host &&
            !panes.value(pane->content->objectName()).toObject().value("floating").toBool()) dock(*pane);
    }
    void restoreLayout() {
        m_saveTimer.stop();
        for(auto pane:{&m_program,&m_source}) if(pane->host && pane->content) {
            pane->host->setProperty("unboundKeepAtop",m_settings.keepAtop(pane->content->objectName()));
            const auto geometry=m_settings.geometry(pane->content->objectName());
            if(!geometry.isEmpty()) {
                if(pane->mainResizeGeometry) pane->mainResizeGeometry->reset(geometry);
                pane->host->restoreGeometry(geometry);
                preserveGeometry(*pane,geometry);
            }
        }
        m_restoreProgram=m_program.content && m_settings.floating(m_program.content->objectName()) && !m_program.host;
        m_restoreSource=m_source.content && m_settings.floating(m_source.content->objectName()) && !m_source.host;
        queueReclaim();
    }
protected:
    bool eventFilter(QObject *object,QEvent *event) override
    {
        for(auto pane:{&m_program,&m_source}) if(object==pane->host && event->type()==QEvent::Close) {
            dock(*pane); return true;
        }
        for(auto pane:{&m_program,&m_source}) if(object==pane->host &&
            (event->type()==QEvent::Move || event->type()==QEvent::Resize || event->type()==QEvent::WindowStateChange))
            m_saveTimer.start();
        if(object==m_page && (event->type()==QEvent::Hide || event->type()==QEvent::Show))
            for(auto pane:{&m_program,&m_source}) if(pane->host) {
                if(event->type()==QEvent::Hide) pane->returningGeometry=pane->host->saveGeometry();
                pane->host->setVisible(event->type()==QEvent::Show);
                if(event->type()==QEvent::Show) preserveGeometry(*pane,pane->mainResizeGeometry
                    ?pane->mainResizeGeometry->intendedGeometry(pane->returningGeometry):pane->returningGeometry);
            }
        if(object==m_program.content && m_program.host && !m_showingProgram) {
            // Single Viewer Mode swaps the native Program/Source widgets,
            // including on Media Pool hover. That mutual exclusion applies to
            // the docked slot, not an independently visible floating Program.
            // Retain native intent for docking, without re-entering hideEvent.
            if(event->type()==QEvent::HideToParent) {
                m_program.hidden=m_program.content->isHidden(); queueReclaim();
            } else if(event->type()==QEvent::ShowToParent) m_program.hidden=false;
        }
        if(object==m_source.content && event->type()==QEvent::HideToParent && m_program.host
            && m_source.content->isHidden()) {
            // Hover exit returns the native single-viewer slot to Program.
            // Resolve's show() can be a no-op because we already kept it shown.
            m_program.hidden=false;
        }
        if ((object==m_source.content || object==m_page) && (event->type()==QEvent::Show ||
            event->type()==QEvent::Hide || event->type()==QEvent::ShowToParent ||
            event->type()==QEvent::HideToParent || event->type()==QEvent::ParentChange)) queueReclaim();
        return false;
    }
private:
    void installButtons(Pane &pane)
    {
        for(auto anchor:pane.content->findChildren<QAbstractButton *>("btnOptions")) {
            auto parent=anchor->parentWidget();
            if(!parent || parent->objectName()!="rightFrame" || !parent->parentWidget()
                || parent->parentWidget()->objectName()!="framePlayerTime") continue;
            auto layout=qobject_cast<QBoxLayout *>(parent->layout());
            if(!layout || layout->indexOf(anchor)<0 || layout->direction()!=QBoxLayout::LeftToRight) continue;
            auto b=new Unbound::UndockButton(parent);
            b->setProperty("unboundPaneName",pane.content->objectName());
            auto p=&pane;
            b->contextMenu=[this,p](QWidget *button,const QPoint &position) {
                if(m_contextMenu) m_contextMenu(button,position);
                for(auto b:p->buttons) if(b) b->acknowledgeDockingFailure();
            };
            b->setObjectName("resolvePaneUndockButton");
            b->setIcon(QIcon(new Unbound::UndockIconEngine)); b->setIconSize(QSize(20,20));
            b->setFixedSize(20,20); b->setCheckable(true); b->setFocusPolicy(Qt::NoFocus);
            b->setStyleSheet("QToolButton { background: transparent; border: none; padding: 0px; }");
            b->setToolTip("Undock"); b->setAccessibleName("Undock");
            connect(b,&QToolButton::clicked,this,[this,p] { toggle(*p); });
            layout->insertWidget(layout->indexOf(anchor),b,0,Qt::AlignVCenter);
            pane.buttons.append(b); b->show();
            m_drags.append(new Unbound::PaneHeaderDrag(parent->parentWidget(),this));
        }
    }
    void toggle(Pane &pane,bool automatic=false)
    {
        pane.dockingGuard->cancel();
        if(pane.host) { dock(pane); return; }
        auto content=pane.content;
        if(!content || !m_page || !m_page->isVisible()) return;
        const auto descriptor=Unbound::PaneCatalogue::find(content->objectName());
        if(!descriptor) return;
        auto parent=content->parentWidget();
        auto layout=parent ? qobject_cast<QBoxLayout *>(parent->layout()) : nullptr;
        const int index=layout ? layout->indexOf(content) : -1;
        if(index<0 || !content->isVisible()) return;
        pane.parent=parent; pane.originalLayout=layout; pane.hidden=content->isHidden();
        pane.stretch=layout->stretch(index); pane.alignment=layout->itemAt(index)->alignment();
        auto placeholder=new QWidget(parent);
        placeholder->setObjectName("unboundViewerPlaceholder");
        placeholder->setFixedSize(0,0); pane.placeholder=placeholder;
        auto item=layout->replaceWidget(content,placeholder,Qt::FindDirectChildrenOnly);
        if(!item) { delete placeholder; pane.placeholder=nullptr; return; }
        delete item;
        auto host=new Unbound::PaneFloatingWindow;
        host->setProperty("unboundKeepAtop",m_settings.keepAtop(content->objectName()));
        host->setProperty("unboundPage",m_page->objectName());
        host->setParent(parent,host->windowFlags());
        Unbound::inheritPaneTaskbarIdentity(host,parent->window());
        host->setObjectName("unboundFloating_"+content->objectName()); host->setWindowTitle(descriptor->label);
        const auto available=Unbound::panePlacementWorkArea(parent);
        host->resize(content->size().expandedTo(QSize(640,400)).boundedTo(available.size()*0.7));
        auto hl=new QVBoxLayout(host); hl->setContentsMargins(0,0,0,0); hl->addWidget(content);
        pane.host=host; host->installEventFilter(this);
        if(&pane==&m_source)
            pane.activationBridge=new Unbound::AuditionActivationBridge(m_main,m_page,content,this,host->objectName());
        Unbound::applyPaneDarkCaption(host,parent->window());
        const auto saved=m_settings.geometry(content->objectName());
        const bool newPlacement=saved.isEmpty() || !host->restoreGeometry(saved);
        if(newPlacement) Unbound::centerNewPane(host,available);
        content->show(); host->show();
        if(newPlacement) Unbound::centerNewPane(host,available);
        else preserveGeometry(pane,saved);
        pane.mainResizeGeometry=new Unbound::PaneMainResizeGeometry(host,m_main,[host](const QByteArray &geometry) {
            host->restoreGeometry(geometry);
        });
        if(!newPlacement) pane.mainResizeGeometry->reset(saved);
        if(!automatic) host->activateWindow();
        for(auto b:pane.buttons) if(b) b->setFloatingState(true);
        check(m_settings.saveFloating(content->objectName(),true));
        remember(pane);
        queueReclaim();
    }
    void preserveGeometry(Pane &pane,const QByteArray &geometry)
    {
        auto host=pane.host;
        if(!host || host->isMaximized() || host->isFullScreen()) return;
        QRect client;
        if(!Unbound::readRectangle(Unbound::qtGeometryJson(geometry).value("clientRect"),client)) return;
        const auto state=host->windowState();
        new Unbound::PaneGeometryTransaction(host,client.size(),[host,state] {
            return host && host->windowState()==state;
        },[host,geometry] {if(host) host->restoreGeometry(geometry);});
    }
    void restoreArea()
    {
        if(m_geometry) m_geometry->setEnabled(false);
    }
    void dock(Pane &pane,bool user=true)
    {
        pane.dockingGuard->cancel();
        if(!pane.host) return;
        if(pane.activationBridge) {
            pane.activationBridge->stop();
            pane.activationBridge->deleteLater();
            pane.activationBridge=nullptr;
        }
        QScopedValueRollback<bool> ownVisibility(m_showingProgram,m_showingProgram || &pane==&m_program);
        remember(pane);
        restoreArea();
        auto host=pane.host;
        delete pane.mainResizeGeometry; pane.mainResizeGeometry=nullptr;
        if(pane.content && pane.originalLayout && pane.placeholder) {
            host->layout()->removeWidget(pane.content);
            auto item=pane.originalLayout->replaceWidget(pane.placeholder,pane.content,Qt::FindDirectChildrenOnly);
            delete item;
            const int index=pane.originalLayout->indexOf(pane.content);
            if(index>=0) { pane.originalLayout->setStretch(index,pane.stretch); pane.originalLayout->setAlignment(pane.content,pane.alignment); }
            delete pane.placeholder.data(); pane.placeholder=nullptr;
            pane.content->setVisible(!pane.hidden);
        }
        host->removeEventFilter(this); host->hide(); host->deleteLater(); pane.host=nullptr;
        for(auto b:pane.buttons) if(b) b->setFloatingState(false);
        if(user && pane.content) {
            check(m_settings.saveFloating(pane.content->objectName(),false));
            if(&pane==&m_program) m_restoreProgram=false; else m_restoreSource=false;
        }
        queueReclaim();
        if(user) {
            auto p=&pane;
            pane.dockingGuard->watch(pane.content,m_page,[this,p](bool failed) {
                if(failed && !p->host && p->content) toggle(*p,true);
                for(auto b:p->buttons) if(b) b->setDockingFailure(failed && p->host
                    ? Unbound::dockingFailureMessage(p->content->objectName()) : QString());
                if(dockingWarningChanged) dockingWarningChanged();
            });
        }
    }
    bool canReclaim() const {
        return m_program.host && m_source.content && (m_source.host || m_source.content->isHidden());
    }
    void queueReclaim() {
        if (m_stopping || m_reclaimPending) return;
        m_reclaimPending=true;
        m_reclaimWork.set("edit_viewer_restoration",m_page);
        QMetaObject::invokeMethod(this,[this] {
            m_reclaimPending=false;
            m_reclaimWork.clear();
            if(m_stopping) return;
            if(m_page && m_page->isVisible()) {
                if(m_restoreProgram && m_program.content && m_program.content->isVisible()) {
                    m_restoreProgram=false; if(!m_program.host) toggle(m_program,true);
                }
                // Native Single Viewer Mode may keep Source unavailable. Retain
                // startup intent until the user enables it, without changing mode.
                if(m_restoreSource && m_source.content && m_source.content->isVisible()) {
                    m_restoreSource=false; if(!m_source.host) toggle(m_source,true);
                }
            }
            if(m_source.host && m_source.content)
                m_source.host->setVisible(m_page && m_page->isVisible() && !m_source.content->isHidden());
            if(m_program.host && m_program.host->isVisible() && m_page && m_page->isVisible()
                && m_program.content && m_program.content->isHidden()) {
                m_showingProgram=true;
                m_program.content->show();
                m_showingProgram=false;
            }
            m_geometry->setEnabled(canReclaim());
        },Qt::QueuedConnection);
    }
    void check(const QString &error) {
        if(error.isEmpty() || m_errorShown) return;
        m_errorShown=true;
        auto box=new QMessageBox(QMessageBox::Warning,"Unbound",error,QMessageBox::Ok,m_main);
        box->setAttribute(Qt::WA_DeleteOnClose); box->setAttribute(Qt::WA_QuitOnClose,false); box->show();
    }
    void remember(Pane &pane) {
        if(!pane.host || !pane.content || pane.host->isMinimized()) return;
        if(pane.host->interactiveDragActive()) { if(!m_stopping) m_saveTimer.start(); return; }
        check(m_settings.save(pane.content->objectName(),pane.host->saveGeometry()));
    }
    void rememberAll() { remember(m_program); remember(m_source); }
    Pane m_program,m_source;
    QPointer<QWidget> m_players,m_page;
    QPointer<QWidget> m_main;
    Unbound::PaneSettings &m_settings;
    QTimer m_saveTimer;
    bool m_restoreProgram=false,m_restoreSource=false,m_stopping=false,m_errorShown=false;
    QList<Unbound::PaneHeaderDrag *> m_drags;
    ViewerGeometryAdapter *m_geometry=nullptr;
    bool m_reclaimPending=false;
    Unbound::LayoutTraceWork m_reclaimWork;
    bool m_showingProgram=false;
    std::function<void(QWidget *,const QPoint &)> m_contextMenu;
};
