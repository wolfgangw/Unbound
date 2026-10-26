// SPDX-License-Identifier: MIT
#include <QApplication>
#include <QAbstractButton>
#include <QTimer>
#include <QMap>
#include <QSet>
#include <QMessageBox>
#include <QChildEvent>
#include <atomic>
#include "panesettings.h"
#include "windowintegration.h"
#include "controller.h"
#include "mainwindowplacement.h"
#include "sidebarfloatsession.h"
#include "panedecoration.h"
#include "editviewers.h"
#include "pagelayoutmenu.h"
#include "pagefocus.h"
#include "gridreferenceevent.h"
#include "panegridreferences.h"

namespace {
namespace Catalogue = Unbound::PaneCatalogue;
bool preservesNativeLayout(const QString &name) {
    const auto pane=Catalogue::find(name);
    return pane && pane->preserveNativeLayout();
}
bool matchesNativeToggle(const QString &name,const QString &buttonName) {
    const auto pane=Catalogue::find(name);
    return pane && pane->toggleNames().contains(buttonName);
}
QString settingsKey(const QString &name) {
    const auto pane=Catalogue::find(name);
    return pane ? pane->settingsKey() : QString();
}
QWidget *panePage(QWidget *pane) {
    const auto entry=Catalogue::find(pane->objectName());
    if(!entry) return nullptr;
    if(const auto control=Catalogue::colorControl(pane->objectName())) {
        bool found=false;
        for(auto p=pane->parentWidget();p;p=p->parentWidget())
            if(p->objectName()==control->scope) { found=true; break; }
        if(!found) return nullptr;
    }
    const auto &rootName=Catalogue::page(entry->page).paneRoot;
    for(auto p=pane->parentWidget();p;p=p->parentWidget())
        if(p->objectName()==rootName) return p;
    return nullptr;
}
class Controller : public QObject
{
public:
    Controller() : QObject(qApp)
    {
        setObjectName(QStringLiteral("UnboundPaneController"));
        m_mainPlacement.error = [this](const QString &message) { settingsError(message); };
        m_names = Catalogue::sidebarNames();
        connect(qApp, &QCoreApplication::aboutToQuit, this, [this] { delete this; }, Qt::DirectConnection);
        qApp->installEventFilter(this);
        // One initial inventory. Later work is restricted to newly added
        // branches and the small set of supported panes/selectors we retain.
        for (auto window : QApplication::topLevelWidgets())
            if (window->objectName() == QLatin1String("UiMainWindow")) indexBranch(window);
        discover();
    }
    ~Controller() override
    {
        if(qApp) qApp->removeEventFilter(this);
        delete m_layoutMenu.data();
        delete m_viewers.data();
        m_mainPlacement.flush();
        m_mainPlacement.error = {};
        m_mainSnap.setWindow(nullptr);
        m_windowGroup.setWindow(nullptr);
        m_pageFocus.attach(nullptr);
        qDeleteAll(m_decorations);
        qDeleteAll(m_sessions);
    }
protected:
    bool eventFilter(QObject *object, QEvent *event) override
    {
        if(object==qApp && event->type()==Unbound::GridReferenceEvent::eventType()) {
            QList<QRect> areas;for(auto screen:QGuiApplication::screens())areas.append(screen->availableGeometry());
            static_cast<Unbound::GridReferenceEvent *>(event)->rectangles=Unbound::gridPageReferences(
                m_settings,currentPage(),areas,[this](const QJsonObject &saved) {
                    return Unbound::Platform::windowPlacementReference(m_window,saved);
                });
            return true;
        }
        switch (event->type()) {
        case QEvent::ChildAdded:
        case QEvent::ChildPolished:
            // ChildAdded can arrive before the child is a complete QWidget.
            // Inspect it only after construction, on this same GUI thread.
            if (object->isWidgetType()) queueBranch(static_cast<QChildEvent *>(event)->child());
            break;
        case QEvent::Show:
        case QEvent::ParentChange:
            if (auto widget=qobject_cast<QWidget *>(object)) {
                const auto name=widget->objectName();
                if (name==QLatin1String("UiMainWindow") && widget!=m_window) queueBranch(widget);
                else if (!Catalogue::pageForRoot(name).isEmpty())
                    { if (!m_pages.contains(widget)) queueBranch(widget); else scheduleDiscovery(); }
                else if (m_names.contains(name)) { indexWidget(widget); scheduleDiscovery(); }
                else if (m_watchedToggles.contains(qobject_cast<QAbstractButton *>(widget))) scheduleDiscovery();
            }
            break;
        case QEvent::LayoutRequest:
            // A supported slot can acquire its layout/content after creation.
            if (m_names.contains(object->objectName())) scheduleDiscovery();
            break;
        case QEvent::Hide:
            if (auto widget=qobject_cast<QWidget *>(object))
                if (widget->isModal()) scheduleDiscovery(); // Resume deferred restoration.
            break;
        default: break; // No work on paint, input, timer or playback events.
        }
        return false;
    }
private:
    QString currentPage() const {
        if(!m_window) return {};
        if(auto stack=m_window->findChild<QStackedWidget *>("widgetStack_Panel"))
            if(auto page=stack->currentWidget()) return page->objectName();
        // Some supported fixtures/early native layouts have no page stack yet.
        for(auto page:m_pages) if(page && page->isVisible()
            && Catalogue::pageForRoot(page->objectName())==page->objectName()) return page->objectName();
        for(auto page:m_pages) if(page && page->isVisible()) {
            const auto key=Catalogue::pageForRoot(page->objectName());
            if(!key.isEmpty()) return key;
        }
        return {};
    }
    QString capturePage(const QString &page) {
        if(!m_settings.readError().isEmpty()) return m_settings.readError();
        if(currentPage()!=page) return QStringLiteral("Return to this page before saving its configuration.");
        m_lastSettingsError.clear();
        m_mainPlacement.refresh(); m_mainPlacement.flush();
        if(page=="WStackPage_Conform" && m_viewers) m_viewers->flushGeometry();
        for(auto it=m_names.cbegin();it!=m_names.cend();++it) {
            const auto key=settingsKey(it.key());
            if(Unbound::PaneSettings::pageForPane(key)!=page) continue;
            const auto session=m_sessions.value(it.key());
            if(session && session->active()) session->rememberGeometry();
            // Preserve still-pending startup intent for a lazily available pane.
            if(!m_settings.floating(key) || m_restoreAttempted.contains(it.key()))
                settingsError(m_settings.saveFloating(key,session && session->active()));
            auto pane=m_candidates.value(it.key());
            if(pane) if(auto toggle=nativeToggle(pane,it.key()))
                settingsError(m_settings.saveToggle(key,toggle->isChecked()));
        }
        return m_lastSettingsError;
    }
    void openLayouts(const QString &page,QWidget *button,const QPoint &position) {
        if(!m_window || currentPage()!=page) return;
        settingsError(capturePage(page));
        delete m_layoutMenu.data();
        auto menu=new Unbound::PageLayoutMenu(m_settings,page,m_window,[this,page] {
            if(currentPage()!=page) return false;
            if(page=="WStackPage_Conform" && m_viewers && m_viewers->hasFloating()) return true;
            for(auto it=m_sessions.cbegin();it!=m_sessions.cend();++it)
                if(Unbound::PaneSettings::pageForPane(settingsKey(it.key()))==page && it.value()->active()) return true;
            return false;
        });
        m_layoutMenu=menu;
        const QPointer<Unbound::UndockButton> warningButton=dynamic_cast<Unbound::UndockButton *>(button);
        menu->dockingWarning=[warningButton] { return warningButton ? warningButton->dockingFailure() : QString(); };
        menu->save=[this,page](QString &name,bool &duplicate) {
            const auto error=capturePage(page);
            return error.isEmpty() ? m_settings.saveLayout(page,name,duplicate) : error;
        };
        menu->recall=[this,page](const QString &id,bool &applied) { return recallLayout(page,id,applied); };
        menu->remove=[this,page](const QString &id) { return m_settings.deleteLayout(page,id); };
        menu->update=[this,page](const QString &id) {
            const auto error=capturePage(page);
            return error.isEmpty() ? m_settings.updateLayout(page,id) : error;
        };
        menu->rename=[this,page](const QString &id,const QString &name) { return m_settings.renameLayout(page,id,name); };
        menu->dockAll=[this,page] { return dockAll(page); };
        menu->popup(button ? button->mapToGlobal(QPoint(0,button->height())) : position,button);
    }
    QString dockAll(const QString &page) {
        if(currentPage()!=page) return QStringLiteral("Return to this page before docking its panels.");
        if(!m_settings.readError().isEmpty()) return m_settings.readError();
        for(const auto &name:m_initializingNative)
            if(Unbound::PaneSettings::pageForPane(settingsKey(name))==page)
                return QStringLiteral("Panels are still initializing. Please try again.");
        for(auto it=m_sessions.cbegin();it!=m_sessions.cend();++it) {
            if(Unbound::PaneSettings::pageForPane(settingsKey(it.key()))!=page || !it.value()->active()) continue;
            const auto error=it.value()->dock(); if(!error.isEmpty()) return error;
        }
        if(page=="WStackPage_Conform" && m_viewers) m_viewers->prepareLayout({});
        // Clear retained startup intent too: a not-yet-created native pane or
        // Source viewer must not float later after the user chose Dock all.
        for(auto it=m_names.cbegin();it!=m_names.cend();++it) {
            const auto key=settingsKey(it.key());
            if(Unbound::PaneSettings::pageForPane(key)!=page) continue;
            const auto error=m_settings.saveFloating(key,false); if(!error.isEmpty()) return error;
            m_pendingLayoutToggles.remove(it.key()); m_pendingColorToggles.remove(it.key());
            m_activationRequested.remove(it.key()); m_restoreAttempted.insert(it.key());
        }
        if(page=="WStackPage_Conform") {
            for(const auto &pane:Catalogue::panes()) {
                if(pane.kind!=Catalogue::Kind::Viewer || Catalogue::page(pane.page).objectName!=page) continue;
                const auto key=pane.settingsKey();
                const auto error=m_settings.saveFloating(key,false); if(!error.isEmpty()) return error;
            }
            if(m_viewers) m_viewers->restoreLayout();
        }
        const auto error=m_settings.clearActiveLayout(page); if(!error.isEmpty()) return error;
        scheduleDiscovery(); return {};
    }
    void normalColorLayout(const QString &page,const QJsonObject &panes)
    {
        if(page!=QLatin1String("WStackPage_Color")
            || panes.value("m_pLightBox").toObject().value("floating").toBool()) return;
        // Lightbox is an implementation detail of floating Clips. Explicitly
        // recalling an ordinary Color layout must not inherit that outgoing
        // session's pre-float mode (manual docking still restores it).
        m_deferredLightbox=nullptr;
        if(!m_window) return;
        const auto buttons=m_window->findChildren<QAbstractButton *>("btnColorToggleLightbox");
        if(buttons.size()==1 && buttons.first()->isEnabled() && buttons.first()->isChecked())
            buttons.first()->click();
    }
    QString recallLayout(const QString &page,const QString &layoutId,bool &applied) {
        applied=false;
        if(currentPage()!=page) return QStringLiteral("Return to this page before recalling its configuration.");
        if(!m_settings.readError().isEmpty()) return m_settings.readError();
        const auto configuration=m_settings.layout(page,layoutId);
        if(configuration.isEmpty()) return QStringLiteral("The panel layout no longer exists.");
        for(const auto &name:m_initializingNative)
            if(Unbound::PaneSettings::pageForPane(settingsKey(name))==page)
                return QStringLiteral("Panels are still initializing. Please try again.");
        const auto captured=capturePage(page);
        if(!captured.isEmpty()) return captured;
        if(m_settings.activeLayout(page)==layoutId && !m_settings.layoutDirty(page,layoutId)) {
            normalColorLayout(page,configuration.value("panes").toObject());
            return {};
        }
        applied=true;
        if(page=="WStackPage_Color") if(auto clips=m_sessions.value("m_pLightBox")) clips->suspendClipsMode(true);
        const auto panes=configuration.value("panes").toObject();
        // Dock only differences, before activating the saved settings: outgoing
        // geometry/dock callbacks must not overwrite the incoming configuration.
        for(auto it=m_sessions.cbegin();it!=m_sessions.cend();++it) {
            if(Unbound::PaneSettings::pageForPane(settingsKey(it.key()))!=page || !it.value()->active()) continue;
            if(!panes.value(it.key()).toObject().value("floating").toBool()) {
                const auto error=it.value()->dock(); if(!error.isEmpty()) return error;
            }
        }
        if(page=="WStackPage_Conform" && m_viewers) m_viewers->prepareLayout(panes);
        const auto error=m_settings.activateLayout(page,layoutId);
        if(!error.isEmpty()) return error;
        normalColorLayout(page,panes);
        for(auto it=m_names.cbegin();it!=m_names.cend();++it) {
            const auto key=settingsKey(it.key());
            if(Unbound::PaneSettings::pageForPane(key)!=page) continue;
            m_pendingDockToggles.remove(it.key()); m_pendingColorToggles.remove(it.key());
            m_activationRequested.remove(it.key()); m_restoreAttempted.remove(it.key());
            if(auto session=m_sessions.value(it.key())) {
                session->applySavedGeometry(m_settings.geometry(key));
                if(session->active()) m_restoreAttempted.insert(it.key());
            }
            if(m_settings.hasToggle(key)) m_pendingLayoutToggles.insert(it.key());
            else m_pendingLayoutToggles.remove(it.key());
        }
        m_mainPlacement.reloadCurrent();
        if(page=="WStackPage_Conform" && m_viewers) m_viewers->restoreLayout();
        scheduleDiscovery();
        return {};
    }
    void restoreLayoutToggles() {
        const auto page=currentPage();
        for(auto it=m_decorations.cbegin();it!=m_decorations.cend();++it) {
            const auto key=settingsKey(it.key());
            if(it.key()==QLatin1String("m_pLightBox") || Unbound::PaneSettings::pageForPane(key)!=page || !m_settings.floating(key) || m_restoreAttempted.contains(it.key())) continue;
            auto pane=it.value()->pane();
            if(pane && panePage(pane) && panePage(pane)->isVisible() && pane->layout() && pane->layout()->count()) return;
        }
        m_restoringToggles=true;
        for(bool checked:{false,true}) for(const auto &name:m_pendingLayoutToggles.values()) {
            const auto key=settingsKey(name);
            if(Unbound::PaneSettings::pageForPane(key)!=page || m_settings.toggle(key)!=checked) continue;
            auto pane=m_candidates.value(name);
            if(!pane) continue;
            auto toggle=nativeToggle(pane,name);
            if(!toggle) continue;
            auto session=m_sessions.value(name);
            if(m_settings.floating(key) && (!session || !session->active())) continue;
            if(toggle->isChecked()!=checked) {
                if(!checked && session && session->active() && !preservesNativeLayout(name)) session->reclaimNativeSpace(toggle);
                else toggle->click();
            }
            m_pendingLayoutToggles.remove(name);
        }
        m_restoringToggles=false;
        if(m_layoutMenu && m_layoutMenu->page()==page) m_layoutMenu->finishRecall();
    }
    void scheduleDiscovery()
    {
        if (m_discoveryQueued || QCoreApplication::closingDown()) return;
        m_discoveryQueued=true;
        QTimer::singleShot(0,this,[this] {
            m_discoveryQueued=false;
            const auto pending=m_pendingBranches;
            m_pendingBranches.clear();
            for (const auto &object : pending) {
                if (!object) continue;
                bool covered=false;
                for (auto parent=object->parent();parent;parent=parent->parent())
                    if (pending.contains(parent) && pending.value(parent)) { covered=true; break; }
                if (!covered) if (auto widget=qobject_cast<QWidget *>(object.data())) indexBranch(widget);
            }
            discover();
        });
    }
    void queueBranch(QObject *object)
    {
        if (!object || QCoreApplication::closingDown()) return;
        m_pendingBranches.insert(object,object);
        scheduleDiscovery();
    }
    void indexBranch(QWidget *root)
    {
        if (root->objectName()==QLatin1String("UiMainWindow") && m_window!=root) {
            m_window=root;
            m_mainSnap.setWindow(root);
            m_windowGroup.setWindow(root);
            m_mainPlacement.attach(root);
            m_pageFocus.attach(root);
        }
        if (!m_window || (root!=m_window && !m_window->isAncestorOf(root))) return;
        indexWidget(root);
        for (auto widget:root->findChildren<QWidget *>()) indexWidget(widget);
    }
    void indexWidget(QWidget *widget)
    {
        if (!m_window || (widget!=m_window && !m_window->isAncestorOf(widget))) return;
        m_mainPlacement.observeWidget(widget);
        if (widget->objectName()==QLatin1String("widgetStack_Panel")) {
            m_mainPlacement.attach(m_window); m_pageFocus.attach(m_window);
        }
        const auto name=widget->objectName();
        if (!Catalogue::pageForRoot(name).isEmpty() && !m_pages.contains(widget)) {
            m_pages.insert(widget);
            connect(widget,&QObject::destroyed,this,[this,widget] { m_pages.remove(widget); });
        }
        if (m_names.contains(name) && panePage(widget)) {
            if (m_candidates.value(name)!=widget) {
                m_candidates.insert(name,widget);
                connect(widget,&QObject::destroyed,this,[this] { scheduleDiscovery(); });
            }
        }
        auto button=qobject_cast<QAbstractButton *>(widget);
        if (!button || m_watchedToggles.contains(button)) return;
        for (auto it=m_names.cbegin();it!=m_names.cend();++it) {
            if (!matchesNativeToggle(it.key(),name)) continue;
            m_toggles[it.key()].append(button);
            m_watchedToggles.insert(button);
            connect(button,&QAbstractButton::clicked,this,[this] { queueRememberUserToggles(); scheduleDiscovery(); });
            connect(button,&QObject::destroyed,this,[this,button] { m_watchedToggles.remove(button); scheduleDiscovery(); });
            if (auto session=m_sessions.value(it.key()))
                if (session->active() && preservesNativeLayout(it.key())) session->observeNativeToggle(button);
            break;
        }
    }
    QAbstractButton *nativeToggle(QWidget *pane, const QString &name)
    {
        // Use Resolve's own toggle to initialize the pane, including lazy
        // content and internal visibility state. Do not unhide descendants:
        // some of them are alternative meter views that must remain hidden.
        QAbstractButton *toggle = nullptr;
        for (auto button : m_toggles.value(name)) {
            if (!button || button->window()!=pane->window() || !button->isVisible()
                || !button->isCheckable() || !button->isEnabled()) continue;
            if (toggle) return nullptr; // Ambiguous UI: never click an arbitrary control.
            toggle = button;
        }
        return toggle;
    }
    void rememberUserToggles()
    {
        if (m_restoringToggles || QCoreApplication::closingDown()) return;
        for (auto it=m_sessions.cbegin(); it!=m_sessions.cend(); ++it) {
            if(m_pendingLayoutToggles.contains(it.key())) continue;
            if (!it.value()->active()) continue;
            auto decoration=m_decorations.value(it.key());
            auto pane=decoration ? decoration->pane() : nullptr;
            if (!pane || !panePage(pane) || !panePage(pane)->isVisible()) continue;
            if (auto toggle=nativeToggle(pane,it.key()))
                settingsError(m_settings.saveToggle(settingsKey(it.key()),toggle->isChecked()));
        }
    }
    void queueRememberUserToggles()
    {
        if (m_restoringToggles) return;
        // Resolve can update exclusive sibling selectors in a queued handler.
        QTimer::singleShot(0,this,[this] { rememberUserToggles(); });
    }
    void restoreColorToggles()
    {
        if (m_pendingColorToggles.isEmpty()) return;
        for (const auto &name : m_pendingColorToggles) {
            auto decoration=m_decorations.value(name);
            auto pane=decoration ? decoration->pane() : nullptr;
            if (!pane || !panePage(pane) || !panePage(pane)->isVisible()) return;
        }
        for (auto it=m_decorations.cbegin();it!=m_decorations.cend();++it) {
            if (it.key()==QLatin1String("m_pLightBox") || !preservesNativeLayout(it.key()) || !m_settings.floating(settingsKey(it.key()))) continue;
            auto pane=it.value()->pane();
            if (pane && panePage(pane)->isVisible() && !m_restoreAttempted.contains(it.key())) return;
        }
        m_restoringToggles=true;
        // Off first, then the selected pane: do not let an off action hide a
        // shared sidebar after its selected pane has already been restored.
        for (bool checked : {false,true}) for (const auto &name : m_pendingColorToggles) {
            if (m_settings.toggle(settingsKey(name))!=checked) continue;
            auto decoration=m_decorations.value(name);
            auto pane=decoration ? decoration->pane() : nullptr;
            if (!pane) continue;
            if (auto toggle=nativeToggle(pane,name))
                if(toggle->isChecked()!=checked) toggle->click();
        }
        m_pendingColorToggles.clear();
        m_restoringToggles=false;
    }
    bool prepareNativePane(QWidget *pane, const QString &name, bool &requested)
    {
        if (m_initializingNative.contains(name)) { requested=true; return false; }
        auto toggle=nativeToggle(pane,name);
        if (!toggle) return false;
        if (!toggle->isChecked()) {
            if (m_activationRequested.contains(name)) return false;
            m_activationRequested.insert(name);
            m_initializingNative.insert(name);
            requested = true;
            m_restoringToggles = true;
            toggle->click();
            m_restoringToggles = false;
            // Other widget events may already have queued discovery ahead of
            // Resolve's toggle handler. Do not detach until that handler ran.
            QTimer::singleShot(0,this,[this,name] {
                m_initializingNative.remove(name);
                scheduleDiscovery();
            });
            return false; // Let Resolve finish its handlers before detaching.
        }
        return true;
    }
    void settingsError(const QString &message)
    {
        if(!message.isEmpty()) m_lastSettingsError=message;
        if (!message.isEmpty() && !m_settingsErrorShown) {
            m_settingsErrorShown = true;
            error(message);
        }
    }
    void showDockedPanes()
    {
        for (const auto &name : m_pendingDockToggles.values()) {
            auto decoration = m_decorations.value(name);
            auto pane = decoration ? decoration->pane() : nullptr;
            auto root = pane ? panePage(pane) : nullptr;
            // A close on an inactive page must not select that page. Apply its
            // native toggle when the user returns and its toolbar is available.
            if (!root || !root->isVisible()) continue;
            if(const auto control=Catalogue::colorControl(name)) {
                if(auto bottom=root->findChild<QWidget *>("frameColorBottom"))
                    for(auto button:bottom->findChildren<QAbstractButton *>(control->selector))
                        if(button->isVisible() && button->isEnabled()) { button->click(); break; }
                m_pendingDockToggles.remove(name);
                continue;
            }
            if (auto toggle = nativeToggle(pane, name)) {
                if (!toggle->isChecked()) toggle->click();
                settingsError(m_settings.saveToggle(settingsKey(name), toggle->isChecked()));
                m_pendingDockToggles.remove(name);
            }
        }
    }
    void undock(const QString &name, bool automatic = false)
    {
        auto decoration = m_decorations.value(name);
        if (!decoration || !decoration->pane()) return;
        auto session = m_sessions.value(name);
        if (!session) {
            session = new Unbound::SidebarFloatSession(this);
            session->setSavedGeometry(m_settings.geometry(settingsKey(name)));
            session->geometrySaved = [this, name](const QByteArray &geometry) {
                settingsError(m_settings.save(settingsKey(name), geometry));
            };
            session->userDocked = [this, name] {
                settingsError(m_settings.saveFloating(settingsKey(name), false));
                // Also suppress retries if saving the user's choice failed.
                m_restoreAttempted.insert(name);
                m_pendingDockToggles.insert(name);
                if (auto decoration=m_decorations.value(name)) decoration->setFloating(false);
                showDockedPanes();
            };
            session->dockingFailed = [this,name] {
                m_pendingDockToggles.remove(name);
                m_pendingLayoutToggles.remove(name); m_pendingColorToggles.remove(name);
                m_restoreAttempted.insert(name);
                settingsError(m_settings.saveFloating(settingsKey(name),true));
                if(auto decoration=m_decorations.value(name)) {
                    decoration->setFloating(true);
                    decoration->setDockingFailure(Unbound::dockingFailureMessage(name));
                    if(auto toggle=nativeToggle(decoration->pane(),name)) {
                        settingsError(m_settings.saveToggle(settingsKey(name),toggle->isChecked()));
                        if(preservesNativeLayout(name)) m_sessions.value(name)->observeNativeToggle(toggle);
                    }
                }
                if(m_layoutMenu) m_layoutMenu->refreshDockingWarning();
            };
            session->dockingSucceeded = [this,name] {
                if(auto decoration=m_decorations.value(name)) decoration->setDockingFailure({});
                if(m_layoutMenu) m_layoutMenu->refreshDockingWarning();
            };
            session->report = [this](const QString &text) { error(text); scheduleDiscovery(); };
            m_sessions.insert(name, session);
        }
        if (session->active()) {
            // Dock explicitly. Resolve can intercept a host Close event before
            // the session sees it, leaving hidden content and a live placeholder.
            // The title-bar Close handler calls this same docking operation.
            if (!automatic) session->dock();
            decoration->setFloating(session->active());
            return;
        }
        m_pendingDockToggles.remove(name);
        auto pane = decoration->pane();
        auto toggle = nativeToggle(pane,name);
        const bool preserveNativeLayout = preservesNativeLayout(name);
        error(session->start(pane, m_names.value(name), panePage(pane),
            name == QStringLiteral("frameEditAudioMeter") || name == QStringLiteral("frameTimelineMixerContainer"), automatic, preserveNativeLayout));
        if (session->active()) {
            // Manual detachment also resolves startup intent. Lifecycle events
            // must not interpret its newly saved floating state as a restore.
            m_restoreAttempted.insert(name);
            if (preserveNativeLayout) for (auto button : m_toggles.value(name))
                if (button) session->observeNativeToggle(button);
            if (toggle) {
                const bool savedChoice=automatic && m_settings.hasToggle(settingsKey(name));
                if (savedChoice && preserveNativeLayout) m_pendingColorToggles.insert(name);
                else if (savedChoice) {
                    m_restoringToggles = true;
                    if (toggle->isChecked() != m_settings.toggle(settingsKey(name))) {
                        if (toggle->isChecked()) session->reclaimNativeSpace(toggle);
                        else toggle->click();
                    }
                    m_restoringToggles = false;
                }
                else if (!automatic && toggle->isChecked()) {
                    // Detach first, then let Resolve collapse the original slot
                    // through its native handler. Never alter a failed detach.
                    session->reclaimNativeSpace(toggle);
                }
                if (!savedChoice) settingsError(m_settings.saveToggle(settingsKey(name),toggle->isChecked()));
            }
            session->rememberGeometry();
            settingsError(m_settings.saveFloating(settingsKey(name), true));
        }
        decoration->setFloating(session->active());
    }
    void error(const QString &text)
    {
        if (text.isEmpty()) return;
        auto box = new QMessageBox(QMessageBox::Warning, QStringLiteral("Unbound"), text, QMessageBox::Ok);
        box->setAttribute(Qt::WA_DeleteOnClose);
        box->setAttribute(Qt::WA_QuitOnClose, false);
        box->show();
    }
    bool pendingColorPanes() const
    {
        // Include not-yet-discovered panes: sorting the current decorations is
        // insufficient when project startup creates native content lazily.
        for(auto it=m_names.cbegin();it!=m_names.cend();++it) {
            if(it.key()==QLatin1String("m_pLightBox")) continue;
            const auto key=settingsKey(it.key());
            if(Unbound::PaneSettings::pageForPane(key)==QLatin1String("WStackPage_Color")
                && m_settings.floating(key) && !m_restoreAttempted.contains(it.key())) return true;
        }
        return false;
    }
    void restoreDeferredLightbox()
    {
        if(!m_deferredLightbox) return;
        auto mode=m_deferredLightbox; m_deferredLightbox=nullptr;
        if(!mode->isChecked()) mode->click();
    }
    void prepareColorRestoration()
    {
        if(currentPage()!=QLatin1String("WStackPage_Color")) return;
        if(!pendingColorPanes()) return;
        if(auto clips=m_sessions.value("m_pLightBox")) {
            if(clips->active()) { clips->suspendClipsMode(true); return; }
        }
        if(!m_window) return;
        const auto buttons=m_window->findChildren<QAbstractButton *>("btnColorToggleLightbox");
        if(buttons.size()!=1) return;
        auto mode=buttons.first();
        if(mode->isVisible() && mode->isEnabled() && mode->isChecked()) {
            // Native Lightbox may be restored by the outgoing Clips dock, even
            // after recall suspended it. Ordinary incoming panels need the
            // toolbar too, whether or not the incoming layout floats Clips.
            // Preserve the native choice until all restoration work finishes.
            m_deferredLightbox=mode;
            mode->click();
        }
    }
    void discover()
    {
        for (auto it = m_decorations.begin(); it != m_decorations.end();) {
            if (!it.value()->pane()) {
                m_activationRequested.remove(it.key());
                m_restoreAttempted.remove(it.key());
                delete it.value(); it = m_decorations.erase(it);
            }
            else { it.value()->refreshNativeHeader(); ++it; }
        }
        for (auto pane : m_candidates) {
            if (!pane) continue;
            const auto name = pane->objectName();
            if (!m_names.contains(name) || m_decorations.contains(name) || !panePage(pane)
                || (!qobject_cast<QBoxLayout *>(pane->layout()) && !qobject_cast<QGridLayout *>(pane->layout()))) continue;
            auto decoration = new Unbound::PaneDecoration(pane, this, [this, name] {
                undock(name);
            },[this,name](QWidget *button,const QPoint &position) {
                openLayouts(Unbound::PaneSettings::pageForPane(settingsKey(name)),button,position);
            });
            m_decorations.insert(name, decoration);
        }
        // Project Manager/other pages can exist before Edit is ready. Keep
        // intent pending, without selecting a page or touching empty layouts.
        if (QCoreApplication::closingDown() || QApplication::activeModalWidget()) return;
        if (!m_viewers && m_window) {
            auto page=m_window->findChild<QWidget *>("WStackPage_Conform");
            auto players=page && page->isVisible() ? page->findChild<QWidget *>("frameEditPageVideoPlayers") : nullptr;
            auto program=players ? players->findChild<QWidget *>("widgetEditTimelinePlayer") : nullptr;
            auto source=players ? players->findChild<QWidget *>("stackedEditSourceOfflinePlayers") : nullptr;
            if (program && source) {
                m_viewers=new EditViewers(m_window,players,program,source,m_settings,this,
                    [this](QWidget *button,const QPoint &position) { openLayouts("WStackPage_Conform",button,position); });
                m_viewers->dockingWarningChanged=[this] { if(m_layoutMenu) m_layoutMenu->refreshDockingWarning(); };
            }
        }
        prepareColorRestoration();
        showDockedPanes();
        auto restoreOrder=m_decorations.keys();
        // Native Lightbox replaces the toolbar. Initialize other Color panes
        // before entering it, so their lazy content and selectors remain usable.
        if(restoreOrder.removeOne("m_pLightBox")) restoreOrder.append("m_pLightBox");
        for (const auto &name : restoreOrder) {
            if (!m_settings.floating(settingsKey(name)) || m_restoreAttempted.contains(name)) continue;
            auto decoration=m_decorations.value(name);
            auto pane = decoration->pane();
            auto root = pane ? panePage(pane) : nullptr;
            if (!root || !root->isVisible() || !pane->layout() || !pane->layout()->count()) continue;
            bool requested = false;
            const auto &descriptor=*Catalogue::find(name);
            if (descriptor.initializeBeforeRestore
                && !prepareNativePane(pane, name, requested)) {
                // Shared sidebar selectors are mutually exclusive. Let one
                // native activation finish and detach it before selecting next.
                if (requested && descriptor.serializeInitialization()) return;
                continue;
            }
            // Fairlight controls belong beside native header actions, never in
            // a fallback strip. Lazy native initialization may create them.
            if (descriptor.requireNativeHeader() && !decoration->usesNativeHeader()) continue;
            if(name==QLatin1String("m_pLightBox")) {
                if(pendingColorPanes()) continue;
                restoreColorToggles(); restoreLayoutToggles();
                restoreDeferredLightbox();
            }
            m_restoreAttempted.insert(name); // No repeated errors for incompatible panes.
            undock(name, true);
        }
        restoreColorToggles();
        restoreLayoutToggles();
        if(!pendingColorPanes()) {
            restoreDeferredLightbox();
            if(auto clips=m_sessions.value("m_pLightBox")) clips->suspendClipsMode(false);
        }
    }
    QPointer<QAbstractButton> m_deferredLightbox;
    QPointer<QWidget> m_window;
    QPointer<EditViewers> m_viewers;
    QPointer<Unbound::PageLayoutMenu> m_layoutMenu;
    QSet<QWidget *> m_pages;
    QMap<QString,QPointer<QWidget>> m_candidates;
    QMap<QString,QList<QPointer<QAbstractButton>>> m_toggles;
    QMap<QObject *,QPointer<QObject>> m_pendingBranches;
    bool m_discoveryQueued=false;
    Unbound::PaneSettings m_settings;
    Unbound::MainWindowPlacement m_mainPlacement{m_settings};
    Unbound::MainWindowSnap m_mainSnap;
    Unbound::WindowGroupActivation m_windowGroup;
    Unbound::PageFocus m_pageFocus;
    bool m_settingsErrorShown = false;
    QString m_lastSettingsError;
    bool m_restoringToggles = false;
    QSet<QAbstractButton *> m_watchedToggles;
    QSet<QString> m_activationRequested;
    QSet<QString> m_initializingNative;
    QSet<QString> m_restoreAttempted;
    QSet<QString> m_pendingColorToggles;
    QSet<QString> m_pendingDockToggles;
    QSet<QString> m_pendingLayoutToggles;
    QMap<QString, QString> m_names;
    QMap<QString, Unbound::PaneDecoration *> m_decorations;
    QMap<QString, Unbound::SidebarFloatSession *> m_sessions;
};
std::atomic<bool> started{false};
}

// Queue all widget work on Resolve's existing GUI thread.
bool Unbound::startController()
{
    auto app = qobject_cast<QApplication *>(QCoreApplication::instance());
    if (!app || QCoreApplication::closingDown()) return 0;
    if (started.exchange(true)) return 1;
    if (!QMetaObject::invokeMethod(app, [app] {
        // The GUI thread owns one controller, including repeated startup requests
        // from separate helper images with independent guards.
        if (!app->findChild<QObject *>(QStringLiteral("UnboundPaneController"))) new Controller;
    }, Qt::QueuedConnection)) {
        started = false;
        return 0;
    }
    return 1;
}
