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

namespace {
QString audioToggle(const QString &name)
{
    static const QMap<QString, QString> panes = {
        {"frameAudioPanelMediaPoolContainer", "MediaPool"},
        {"frameAudioPanelEffectsLibraryContainer", "EffectsLibrary"},
        {"frameAudioPanelIndexContainer", "Index"},
        {"frameAudioPanelGroupsContainer", "Groups"},
        {"frameAudioPanelSoundLibraryContainer", "SoundLibrary"},
        {"frameAudioPanelAdrContainer", "Adr"},
        {"frameTimelineMixerContainer", "Mixer"},
        {"frameMetadataContainer", "Metadata"}, {"frameInspectorContainer", "Inspector"}};
    return panes.value(name);
}
QString colorToggle(const QString &name)
{
    if (name == QLatin1String("m_pFrameNodeGraph")) return QStringLiteral("btnColorToggleNodeGraph");
    if (name == QLatin1String("m_pWidgetGallery")) return QStringLiteral("btnColorToggleGallery");
    if (name == QLatin1String("m_pWidgetLutBrowser")) return QStringLiteral("btnColorToggleLutsBrowser");
    if (name == QLatin1String("m_pWidgetMediaPool")) return QStringLiteral("btnColorToggleMediaPool");
    return {};
}
bool matchesColorToggle(const QString &name, const QString &buttonName)
{
    const auto base=colorToggle(name);
    return !base.isEmpty() && (buttonName==base || buttonName==base+QStringLiteral("InDSM"));
}
bool matchesNativeToggle(const QString &name, const QString &buttonName)
{
    if (!colorToggle(name).isEmpty()) return matchesColorToggle(name, buttonName);
    if (!audioToggle(name).isEmpty()) {
        const auto base = QStringLiteral("btnAudioPanel") + audioToggle(name);
        return buttonName == base + QStringLiteral("InSSM") || buttonName == base + QStringLiteral("InDSM");
    }
    static const QMap<QString, QString> editToggles = {
        {"frameEditPageMediaPool", "MediaPool"}, {"frameEditPageEffectsLibrary", "EffectsLibrary"},
        {"frameEditPageSoundLibrary", "SoundLibrary"}, {"frameEditPageKeyframeEditor", "KeyframeEditor"},
        {"frameEditPageEditIndex", "EditIndex"}, {"frameEditPageInspector", "Inspector"},
        {"frameEditPageMetadata", "Metadata"}, {"frameEditAudioMeter", "Mixer"}};
    const auto suffix = editToggles.value(name);
    if (suffix.isEmpty()) return false;
    const auto base = QStringLiteral("btnEditToggle") + suffix;
    return buttonName == base + QStringLiteral("InSSM")
        || buttonName == base + QStringLiteral("InDSM")
        || buttonName == base + QStringLiteral("InEFTM");
}
QString settingsKey(const QString &name)
{
    // Preserve existing Edit keys; new pages use an explicit namespace.
    if (!audioToggle(name).isEmpty()) return QStringLiteral("Audio/") + name;
    return !colorToggle(name).isEmpty() ? QStringLiteral("Color/") + name : name;
}
QWidget *panePage(QWidget *pane)
{
    const auto rootName = !audioToggle(pane->objectName()).isEmpty() ? QStringLiteral("WStackPage_Audio")
        : !colorToggle(pane->objectName()).isEmpty()
        ? QStringLiteral("WStackPage_Color") : QStringLiteral("frameEditPageWidgetsContainer");
    for (auto p = pane->parentWidget(); p; p = p->parentWidget())
        if (p->objectName() == rootName) return p;
    return nullptr;
}
class Controller : public QObject
{
public:
    Controller() : QObject(qApp)
    {
        setObjectName(QStringLiteral("UnboundPaneController"));
        m_mainPlacement.error = [this](const QString &message) { settingsError(message); };
        m_names = {{"frameEditPageMediaPool", "Media Pool"}, {"frameEditPageSoundLibrary", "Sound Library"},
            {"frameEditPageKeyframeEditor", "Keyframe Editor"}, {"frameEditPageEffectsLibrary", "Effects"},
            {"frameEditPageEditIndex", "Edit Index"}, {"frameEditPageInspector", "Inspector"},
            {"frameEditPageMetadata", "Metadata"}, {"frameEditAudioMeter", "Mixer"},
            {"m_pFrameNodeGraph", "Nodes"}, {"m_pWidgetGallery", "Gallery"},
            {"m_pWidgetLutBrowser", "LUTs"}, {"m_pWidgetMediaPool", "Media Pool"},
            {"frameAudioPanelMediaPoolContainer", "Media Pool"}, {"frameAudioPanelEffectsLibraryContainer", "Effects"},
            {"frameAudioPanelIndexContainer", "Index"}, {"frameAudioPanelGroupsContainer", "Groups"},
            {"frameAudioPanelSoundLibraryContainer", "Sound Library"}, {"frameAudioPanelAdrContainer", "ADR"},
            {"frameTimelineMixerContainer", "Mixer"}, {"frameMetadataContainer", "Metadata"},
            {"frameInspectorContainer", "Inspector"}};
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
        delete m_viewers.data();
        m_mainPlacement.flush();
        m_mainPlacement.error = {};
        m_mainSnap.setWindow(nullptr);
        m_windowGroup.setWindow(nullptr);
        qDeleteAll(m_decorations);
        qDeleteAll(m_sessions);
    }
protected:
    bool eventFilter(QObject *object, QEvent *event) override
    {
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
                else if (name==QLatin1String("frameEditPageWidgetsContainer")
                    || name==QLatin1String("WStackPage_Conform") || name==QLatin1String("WStackPage_Color")
                    || name==QLatin1String("WStackPage_Audio"))
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
        }
        if (!m_window || (root!=m_window && !m_window->isAncestorOf(root))) return;
        indexWidget(root);
        for (auto widget:root->findChildren<QWidget *>()) indexWidget(widget);
    }
    void indexWidget(QWidget *widget)
    {
        if (!m_window || (widget!=m_window && !m_window->isAncestorOf(widget))) return;
        m_mainPlacement.observeWidget(widget);
        if (widget->objectName()==QLatin1String("widgetStack_Panel")) m_mainPlacement.attach(m_window);
        const auto name=widget->objectName();
        if ((name==QLatin1String("frameEditPageWidgetsContainer") || name==QLatin1String("WStackPage_Conform")
            || name==QLatin1String("WStackPage_Color") || name==QLatin1String("WStackPage_Audio")) && !m_pages.contains(widget)) {
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
                if (session->active() && !colorToggle(it.key()).isEmpty()) session->observeNativeToggle(button);
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
            if (colorToggle(it.key()).isEmpty() || !m_settings.floating(settingsKey(it.key()))) continue;
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
        const bool preserveNativeLayout = !colorToggle(name).isEmpty();
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
    void discover()
    {
        for (auto it = m_decorations.begin(); it != m_decorations.end();) {
            if (!it.value()->pane()) {
                m_activationRequested.remove(it.key());
                m_restoreAttempted.remove(it.key());
                delete it.value(); it = m_decorations.erase(it);
            }
            else ++it;
        }
        for (auto pane : m_candidates) {
            if (!pane) continue;
            const auto name = pane->objectName();
            if (!m_names.contains(name) || m_decorations.contains(name) || !panePage(pane)
                || (!qobject_cast<QBoxLayout *>(pane->layout()) && !qobject_cast<QGridLayout *>(pane->layout()))) continue;
            auto decoration = new Unbound::PaneDecoration(pane, this, [this, name] {
                undock(name);
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
                m_viewers=new EditViewers(m_window,players,program,source,m_settings,this);
            }
        }
        showDockedPanes();
        for (auto it = m_decorations.cbegin(); it != m_decorations.cend(); ++it) {
            const auto name = it.key();
            if (!m_settings.floating(settingsKey(name)) || m_restoreAttempted.contains(name)) continue;
            auto pane = it.value()->pane();
            auto root = pane ? panePage(pane) : nullptr;
            if (!root || !root->isVisible() || !pane->layout() || !pane->layout()->count()) continue;
            bool requested = false;
            if ((name == QStringLiteral("frameEditAudioMeter") || name == QStringLiteral("frameEditPageInspector")
                || name == QStringLiteral("frameEditPageMetadata") || !colorToggle(name).isEmpty() || !audioToggle(name).isEmpty())
                && !prepareNativePane(pane, name, requested)) {
                // Shared sidebar selectors are mutually exclusive. Let one
                // native activation finish and detach it before selecting next.
                if (requested && (!colorToggle(name).isEmpty() || !audioToggle(name).isEmpty())) return;
                continue;
            }
            // Fairlight controls belong beside native header actions, never in
            // a fallback strip. Lazy native initialization may create them.
            if (!audioToggle(name).isEmpty() && !it.value()->usesNativeHeader()) continue;
            m_restoreAttempted.insert(name); // No repeated errors for incompatible panes.
            undock(name, true);
        }
        restoreColorToggles();
    }
    QPointer<QWidget> m_window;
    QPointer<EditViewers> m_viewers;
    QSet<QWidget *> m_pages;
    QMap<QString,QPointer<QWidget>> m_candidates;
    QMap<QString,QList<QPointer<QAbstractButton>>> m_toggles;
    QMap<QObject *,QPointer<QObject>> m_pendingBranches;
    bool m_discoveryQueued=false;
    Unbound::PaneSettings m_settings;
    Unbound::MainWindowPlacement m_mainPlacement{m_settings};
    Unbound::MainWindowSnap m_mainSnap;
    Unbound::WindowGroupActivation m_windowGroup;
    bool m_settingsErrorShown = false;
    bool m_restoringToggles = false;
    QSet<QAbstractButton *> m_watchedToggles;
    QSet<QString> m_activationRequested;
    QSet<QString> m_initializingNative;
    QSet<QString> m_restoreAttempted;
    QSet<QString> m_pendingColorToggles;
    QSet<QString> m_pendingDockToggles;
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
    if (!QMetaObject::invokeMethod(app, [] { new Controller; }, Qt::QueuedConnection)) {
        started = false;
        return 0;
    }
    return 1;
}
