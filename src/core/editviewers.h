// SPDX-License-Identifier: MIT
#pragma once
#include <QApplication>
#include <QBoxLayout>
#include <QCloseEvent>
#include <QMessageBox>
#include <QPointer>
#include <QResizeEvent>
#include <QScopedValueRollback>
#include <QTimer>
#include "windowintegration.h"
#include "panedecoration.h"
#include "viewergeometryadapter.h"
#include "panesettings.h"

// Whole native viewer widgets retain their layout slots while floating.
// Share the controller's settings instance so saves cannot overwrite each other.
class EditViewers : public QObject
{
    struct Pane {
        QPointer<QWidget> content, parent, placeholder;
        QPointer<QBoxLayout> originalLayout;
        QPointer<Unbound::PaneFloatingWindow> host;
        QList<QPointer<Unbound::UndockButton>> buttons;
        int stretch=0;
        Qt::Alignment alignment;
        bool hidden=false;
    };
public:
    EditViewers(QWidget *main, QWidget *players, QWidget *program, QWidget *source,
        Unbound::PaneSettings &settings, QObject *owner=nullptr)
        : QObject(owner ? owner : main), m_players(players), m_main(main), m_settings(settings)
    {
        setObjectName("UnboundEditViewers");
        m_program.content=program; m_source.content=source;
        m_page=players;
        while (m_page && m_page->objectName()!="WStackPage_Conform") m_page=m_page->parentWidget();
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
        if(m_page) m_page->removeEventFilter(this);
        qDeleteAll(m_drags);
        for (auto pane:{&m_program,&m_source}) for(auto b:pane->buttons) delete b.data();
    }
    void toggleProgram() { toggle(m_program,"Program Viewer"); }
    void toggleSource() { toggle(m_source,"Source Viewer"); }
    void dockBoth() { restoreArea(); dock(m_source); dock(m_program); }
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
            for(auto pane:{&m_program,&m_source}) if(pane->host) pane->host->setVisible(event->type()==QEvent::Show);
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
            b->setObjectName("resolvePaneUndockButton");
            b->setIcon(QIcon(new Unbound::UndockIconEngine)); b->setIconSize(QSize(20,20));
            b->setFixedSize(20,20); b->setCheckable(true); b->setFocusPolicy(Qt::NoFocus);
            b->setStyleSheet("QToolButton { background: transparent; border: none; padding: 0px; }");
            b->setToolTip("Undock"); b->setAccessibleName("Undock");
            auto p=&pane;
            connect(b,&QToolButton::clicked,this,[this,p] { toggle(*p,p==&m_program ? "Program Viewer" : "Source Viewer"); });
            layout->insertWidget(layout->indexOf(anchor),b,0,Qt::AlignVCenter);
            pane.buttons.append(b); b->show();
            m_drags.append(new Unbound::PaneHeaderDrag(parent->parentWidget(),this));
        }
    }
    void toggle(Pane &pane,const QString &title)
    {
        if(pane.host) { dock(pane); return; }
        auto content=pane.content;
        if(!content || !m_page || !m_page->isVisible()) return;
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
        host->setParent(parent,host->windowFlags());
        Unbound::inheritPaneTaskbarIdentity(host,parent->window());
        host->setObjectName("unboundFloating_"+content->objectName()); host->setWindowTitle(title);
        const auto available=QGuiApplication::primaryScreen()->availableGeometry();
        host->resize(content->size().expandedTo(QSize(640,400)).boundedTo(available.size()*0.7));
        auto hl=new QVBoxLayout(host); hl->setContentsMargins(0,0,0,0); hl->addWidget(content);
        pane.host=host; host->installEventFilter(this);
        Unbound::applyPaneDarkCaption(host,parent->window());
        const auto saved=m_settings.geometry(content->objectName());
        if(saved.isEmpty() || !host->restoreGeometry(saved))
            host->move(available.center()-QPoint(host->width()/2,host->height()/2));
        content->show(); host->show();
        for(auto b:pane.buttons) if(b) { b->setChecked(true); b->setToolTip("Click to dock; drag to move"); b->setAccessibleName("Dock"); }
        check(m_settings.saveFloating(content->objectName(),true));
        remember(pane);
        queueReclaim();
    }
    void restoreArea()
    {
        if(m_geometry) m_geometry->setEnabled(false);
    }
    void dock(Pane &pane,bool user=true)
    {
        if(!pane.host) return;
        QScopedValueRollback<bool> ownVisibility(m_showingProgram,m_showingProgram || &pane==&m_program);
        remember(pane);
        restoreArea();
        auto host=pane.host;
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
        for(auto b:pane.buttons) if(b) { b->setChecked(false); b->setToolTip("Undock"); b->setAccessibleName("Undock"); }
        if(user && pane.content) {
            check(m_settings.saveFloating(pane.content->objectName(),false));
            if(&pane==&m_program) m_restoreProgram=false; else m_restoreSource=false;
        }
        queueReclaim();
    }
    bool canReclaim() const {
        return m_program.host && m_source.content && (m_source.host || m_source.content->isHidden());
    }
    void queueReclaim() {
        if (m_stopping || m_reclaimPending) return;
        m_reclaimPending=true;
        QMetaObject::invokeMethod(this,[this] {
            m_reclaimPending=false;
            if(m_stopping) return;
            if(m_page && m_page->isVisible()) {
                if(m_restoreProgram && m_program.content && m_program.content->isVisible()) {
                    m_restoreProgram=false; if(!m_program.host) toggleProgram();
                }
                // Native Single Viewer Mode may keep Source unavailable. Retain
                // startup intent until the user enables it, without changing mode.
                if(m_restoreSource && m_source.content && m_source.content->isVisible()) {
                    m_restoreSource=false; if(!m_source.host) toggleSource();
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
    bool m_showingProgram=false;
};
