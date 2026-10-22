// SPDX-License-Identifier: MIT
#ifndef UNBOUND_SIDEBARFLOATSESSION_H
#define UNBOUND_SIDEBARFLOATSESSION_H

#include <QBoxLayout>
#include <QAbstractButton>
#include <QGridLayout>
#include <QCloseEvent>
#include <QLabel>
#include <QPointer>
#include <QPropertyAnimation>
#include <QTimer>
#include <QSplitter>
#include <QWidget>
#include <functional>
#include "windowintegration.h"
#include "nodescanvasbounds.h"
#include "auditionactivationbridge.h"
#include "paneheaderdrag.h"
#include "panecatalogue.h"
#include "panedockingguard.h"

namespace Unbound {
// Move the entire layout, including all widgets/spacers/stretch factors. This
// preserves multi-part panes such as Edit Index without guessing a content child.
// A hidden outer slot is normal when the user selects another sidebar toggle.
class SidebarFloatSession : public QObject
{
public:
    explicit SidebarFloatSession(QObject *parent = nullptr) : QObject(parent)
    {
        m_geometryTimer = new QTimer(this);
        m_geometryTimer->setSingleShot(true);
        m_geometryTimer->setInterval(750);
        connect(m_geometryTimer, &QTimer::timeout, this, [this] { rememberGeometry(); });
        m_returnFade = new QPropertyAnimation(this);
        m_returnFade->setPropertyName("windowOpacity");
        m_returnFade->setDuration(150);
        m_returnFade->setStartValue(0.0);
        m_returnFade->setEndValue(1.0);
        m_returnFade->setEasingCurve(QEasingCurve::OutCubic);
    }
    ~SidebarFloatSession() override
    {
        m_dockingGuard.cancel();
        report = {};
        restore();
        if (m_host) delete m_host.data();
    }
    std::function<void(const QString &)> report;
    std::function<void(const QByteArray &)> geometrySaved;
    std::function<void()> userDocked;
    std::function<void()> dockingFailed;
    std::function<void()> dockingSucceeded;
    void setSavedGeometry(const QByteArray &geometry) { if (!active()) m_savedGeometry = geometry; }
    void applySavedGeometry(const QByteArray &geometry) {
        m_geometryTimer->stop(); m_savedGeometry=geometry;
        if(m_host && !geometry.isEmpty()) static_cast<PaneFloatingWindow *>(m_host.data())->restoreGeometry(geometry);
    }
    void rememberGeometry()
    {
        m_geometryTimer->stop();
        if (!m_host) return;
        if (static_cast<PaneFloatingWindow *>(m_host.data())->interactiveDragActive()) {
            m_geometryTimer->start();
            return;
        }
        // Keep the normal/maximized placement, never persist a minimized startup.
        if (m_host->isMinimized()) return;
        m_savedGeometry = m_host->saveGeometry();
        if (geometrySaved) geometrySaved(m_savedGeometry);
    }
    bool active() const { return !m_host.isNull(); }
    QWidget *floatingWindow() const { return m_host.data(); }
    QString dock()
    {
        if (!active() || m_restoring) return QString();
        const auto slot=m_slot, root=m_root;
        const auto label=m_host->windowTitle();
        const bool splitter=bool(m_splitter), native=bool(m_nativeLayout);
        const auto error = restore();
        if (error.isEmpty()) {
            if(userDocked) userDocked();
            m_dockingGuard.watch(slot,root,[this,slot,root,label,splitter,native](bool failed) {
                if(!failed) { if(dockingSucceeded) dockingSucceeded(); return; }
                if(!slot || !root || active()) return;
                // Use the same reversible transfer and the last floating geometry.
                // Automatic recovery must not activate a window or change page.
                const auto error=start(slot,label,root,splitter,false,native,false);
                if(error.isEmpty()) { if(dockingFailed) dockingFailed(); }
                else if(report) report(error);
            });
        }
        if (report) report(error);
        return error;
    }
    void reclaimNativeSpace(QAbstractButton *button)
    {
        if (!m_host || !button || !button->isChecked()) return;
        const auto host = m_host;
        const auto generation = m_generation;
        // Fairlight's Mixer toggle can hide the transferred content's current
        // container (our host), whereas child-only recovery suffices for Edit.
        const bool restoreHost = m_slot && m_slot->objectName()==QLatin1String("frameTimelineMixerContainer")
            && host->isVisible();
        QList<QPointer<QWidget>> visibleContent;
        // Some Edit handlers hide their retained content pointers as well as
        // the native slot. Preserve only the floating layout's visible children;
        // explicitly hidden alternate views must stay hidden.
        if (!m_nativeLayout) for (auto child : host->findChildren<QWidget *>(QString(), Qt::FindDirectChildrenOnly))
            if (!child->isHidden()) visibleContent.append(child);
        button->click();
        // Let Resolve's queued toggle handler finish before restoring content.
        // Parent checks also make a quick dock safe before this callback runs.
        QTimer::singleShot(0, this, [this, host, visibleContent, generation, restoreHost] {
            if (!host || m_host!=host || m_generation!=generation) return;
            for (auto child : visibleContent)
                if (child && child->parentWidget() == host) child->show();
            if (restoreHost && m_root && !m_root->isVisible()) {
                m_hiddenByPage=true; // The native hide may precede page Hide.
            } else if (restoreHost && m_root && !host->isVisible()) {
                host->show();
            }
        });
    }
    void observeNativeToggle(QAbstractButton *button)
    {
        if (!active() || !button || !m_nativeLayout) return;
        m_nativeToggleConnections.append(connect(button, &QAbstractButton::clicked, this,
            [this] { queueGraphRecovery(); }));
        m_nativeToggleConnections.append(connect(button, &QAbstractButton::toggled, this,
            [this] { queueGraphRecovery(); }));
    }

    QString start(QWidget *slot, const QString &label, QWidget *pageRoot = nullptr, bool reclaimSplitterSpace = false, bool fadeIn = false, bool preserveNativeLayout = false, bool activate = true)
    {
        m_dockingGuard.cancel();
        if (active()) return QStringLiteral("This pane is already floating.");
        if (!slot || slot->isWindow() || !slot->parentWidget() || !(pageRoot ? pageRoot : slot->parentWidget())->isVisible())
            return QStringLiteral("Open this pane's Resolve page before undocking it.");
        auto layout = slot->layout();
        if (!layout || (!qobject_cast<QBoxLayout *>(layout) && !qobject_cast<QGridLayout *>(layout)) || layout->count() == 0)
            return QStringLiteral("Expected a nonempty box or grid layout; no changes made.");
        auto nativeGrid = qobject_cast<QGridLayout *>(layout);
        if (preserveNativeLayout && (layout->count() != 1 || !layout->itemAt(0)->widget()))
            return QStringLiteral("Expected a layout containing one content widget; no changes made.");
        auto splitter = qobject_cast<QSplitter *>(slot->parentWidget());
        if (reclaimSplitterSpace && (!splitter || splitter->indexOf(slot) < 0))
            return QStringLiteral("Expected Mixer inside a QSplitter; no changes made.");
        if (reclaimSplitterSpace) {
            m_splitter = splitter;
            m_splitterSizes = splitter->sizes();
            for (int i = 0; i < splitter->count(); ++i) m_splitterWidgets.append(splitter->widget(i));
            m_slotWasHidden = slot->isHidden();
        }
        ++m_generation;
        m_slot = slot;
        m_root = pageRoot ? pageRoot : slot->parentWidget();
        m_layout = layout;
        auto host = new PaneFloatingWindow;
        host->setProperty("unboundPage",PaneCatalogue::pageForRoot(m_root->objectName()));
        // Keep the Fairlight Media Pool's original Qt ownership/ancestor path
        // while its host remains an independent top-level window.
        if (slot->objectName()==QLatin1String("frameAudioPanelMediaPoolContainer"))
            host->setParent(slot, host->windowFlags());
        inheritPaneTaskbarIdentity(host, slot->window());
        m_host = host;
        host->setObjectName(QStringLiteral("unboundFloating_") + slot->objectName());
        host->setWindowTitle(label);
        host->resize(slot->size().expandedTo(QSize(360, 300)));
        if (preserveNativeLayout) {
            // Retain Resolve's native layout during visibility/selector changes.
            // Move only its sole content widget and preserve its layout slot.
            m_nativeLayout = layout;
            auto item = layout->itemAt(0);
            m_content = item->widget();
            m_contentAlignment = item->alignment();
            m_contentWasHidden = m_content->isHidden();
            if (nativeGrid) nativeGrid->getItemPosition(0, &m_gridRow, &m_gridColumn, &m_gridRows, &m_gridColumns);
            else m_boxStretch=static_cast<QBoxLayout *>(layout)->stretch(0);
            layout->removeWidget(m_content);
            auto hostLayout = new QVBoxLayout(host);
            hostLayout->setContentsMargins(0, 0, 0, 0);
            hostLayout->addWidget(m_content);
            m_content->show();
            m_hostLayout = hostLayout;
            auto placeholder = new QLabel(QStringLiteral("%1 is floating.").arg(label), slot);
            placeholder->setObjectName(QStringLiteral("unboundSidebarPlaceholder"));
            placeholder->setAlignment(Qt::AlignCenter);
            m_placeholder = placeholder;
            addNativeContent(placeholder);
            placeholder->show();
            m_content->installEventFilter(this);
        } else {
        // QWidget::setLayout takes the existing layout off its previous widget
        // and reparents its child widgets into the new host.
        host->setLayout(layout);
        if (slot->layout() || host->layout() != layout) {
            if (!slot->layout() && host->layout() == layout) slot->setLayout(layout);
            delete host;
            clear();
            return QStringLiteral("Could not transfer the pane layout.");
        }
        auto placeholderLayout = new QVBoxLayout(slot);
        m_placeholderLayout = placeholderLayout;
        auto placeholder = new QLabel(QStringLiteral("%1 is floating.").arg(label), slot);
        m_placeholder = placeholder;
        placeholder->setObjectName(QStringLiteral("unboundSidebarPlaceholder"));
        placeholder->setAlignment(Qt::AlignCenter);
        placeholderLayout->addWidget(placeholder);
        placeholder->show();
        }
        if (slot->objectName()==QLatin1String("frameEditPageInspector") || slot->objectName()==QLatin1String("frameInspectorContainer")) {
            auto header=host->findChild<QWidget *>(QStringLiteral("frameEffectsWidgetsTitle"));
            auto title=header ? header->findChild<QLabel *>(QStringLiteral("labelTitle"),Qt::FindDirectChildrenOnly) : nullptr;
            // Resolve already paints this label with middle elision. Its full
            // text minimum-size hint must not become a top-level window minimum.
            if (title && title->property("elideMode").isValid()
                && title->property("elideMode").toInt()==int(Qt::ElideMiddle)) {
                m_inspectorTitle=title;
                m_inspectorTitlePolicy=title->sizePolicy();
                auto policy=m_inspectorTitlePolicy;
                // Qt requires the canonical Ignored policy to discard the hint.
                // Stretch still lets the title use available header space.
                policy.setHorizontalPolicy(QSizePolicy::Ignored);
                policy.setHorizontalStretch(qMax(1,policy.horizontalStretch()));
                title->setSizePolicy(policy);
                // Recompute before restoring geometry/showing the host, rather
                // than waiting for queued layout requests with the old minimum.
                for (auto parent=title->parentWidget(); parent; parent=parent->parentWidget()) {
                    if (parent->layout()) parent->layout()->invalidate();
                    parent->updateGeometry();
                    if (parent==host) break;
                }
                host->layout()->activate();
            }
        }
        const bool fairlightAudition = slot->objectName() == QLatin1String("frameAudioPanelMediaPoolContainer")
            && m_root->objectName() == QLatin1String("WStackPage_Audio");
        const bool soundLibraryAudition = slot->objectName() == QLatin1String("frameEditPageSoundLibrary")
            && m_root->objectName() == QLatin1String("frameEditPageWidgetsContainer");
        if ((fairlightAudition || soundLibraryAudition)
            && slot->window()->objectName() == QLatin1String("UiMainWindow")) {
            // One targeted lookup inside this pane per detach, before its first
            // activation (also for startup restoration). No recurring discovery.
            if (auto preview = host->findChild<QWidget *>(soundLibraryAudition
                    ? QStringLiteral("m_PlayerPreview") : QStringLiteral("previewPlayer"))) {
                m_auditionBridge = new AuditionActivationBridge(slot->window(), m_root, preview, this, host->objectName());
                if (auto header = fairlightAudition ? preview->findChild<QWidget *>(QStringLiteral("framePlayerTime"), Qt::FindDirectChildrenOnly) : nullptr)
                    m_auditionHeaderDrag = new PaneHeaderDrag(header, this);
            }
        }
        host->installEventFilter(this);
        m_root->installEventFilter(this);
        // If Resolve deletes the original slot, dispose only of our displaced
        // content: it would otherwise have been owned by that deleted slot.
        m_destroyConnection = connect(slot, &QObject::destroyed, this, [this]() {
            if (m_root) m_root->removeEventFilter(this);
            if (m_host) { m_host->hide(); m_host->deleteLater(); }
            clear();
        });
        if (m_splitter) {
            slot->installEventFilter(this);
            slot->hide();
        }
        if (!m_savedGeometry.isEmpty()) host->restoreGeometry(m_savedGeometry);
        applyPaneDarkCaption(host, slot->window());
        if (fadeIn) {
            m_returnFade->setTargetObject(host);
            host->setWindowOpacity(0.0);
        }
        host->show();
        if(!fadeIn && activate) host->activateWindow(); // Explicit manual undocking.
        if (preserveNativeLayout && slot->objectName()==QStringLiteral("m_pFrameNodeGraph"))
            m_nodesBounds=new NodesCanvasBounds(m_content,this);
        if (fadeIn) {
            host->repaint();
            m_returnFade->start();
        }
        return QString();
    }

    QString restore()
    {
        m_dockingGuard.cancel();
        if (!active()) return QString();
        if (m_restoring) return QString();
        // A close can arrive before the queued native-toggle recovery.
        recoverFloatingGraph();
        const bool layoutIntact = m_nativeLayout
            ? (m_content && m_hostLayout && m_slot && m_slot->layout() == m_nativeLayout
                && m_host->layout() == m_hostLayout && m_content->parentWidget() == m_host)
            : (m_slot && m_layout && m_placeholderLayout && m_slot->layout() == m_placeholderLayout
                && m_host->layout() == m_layout);
        if (!layoutIntact || !m_placeholder)
            return QStringLiteral("Resolve changed this pane's layout. Restart Resolve to reset the experiment.");
        static_cast<PaneFloatingWindow *>(m_host.data())->finishDeferredResize();
        rememberGeometry();
        m_restoring = true;
        stopAuditionBridge();
        restoreInspectorTitlePolicy();
        if (m_root) m_root->removeEventFilter(this);
        disconnect(m_destroyConnection);
        m_host->removeEventFilter(this);
        if (m_nativeLayout) {
            m_content->removeEventFilter(this);
            m_hostLayout->removeWidget(m_content);
            delete m_placeholder.data();
            addNativeContent(m_content);
            m_content->setVisible(!m_contentWasHidden);
        } else {
            delete m_placeholderLayout.data();
            delete m_placeholder.data();
            m_slot->setLayout(m_layout);
        }
        m_layout->activate();
        if (m_splitter) {
            m_slot->removeEventFilter(this);
            m_slot->setVisible(!m_slotWasHidden);
            bool sameChildren = m_splitter->count() == m_splitterWidgets.size();
            for (int i = 0; sameChildren && i < m_splitterWidgets.size(); ++i)
                sameChildren = m_splitterWidgets[i] && m_splitter->widget(i) == m_splitterWidgets[i];
            if (sameChildren) m_splitter->setSizes(m_splitterSizes);
        }
        // Other sidebar slots retain their current toolbar visibility.
        auto host = m_host.data();
        clear();
        host->hide();
        host->deleteLater();
        m_restoring = false;
        return QString();
    }

protected:
    bool eventFilter(QObject *object, QEvent *event) override
    {
        if (object == m_content && !m_restoring && !m_recoveringGraph
            && (event->type() == QEvent::ParentChange || event->type() == QEvent::Hide))
            queueGraphRecovery();
        if (object == m_host && (event->type() == QEvent::Move || event->type() == QEvent::Resize
            || event->type() == QEvent::WindowStateChange)) m_geometryTimer->start();
        if (object == m_root && !m_restoring && m_host) {
            if (event->type() == QEvent::Hide) {
                m_hiddenByPage = m_hiddenByPage || m_host->isVisible();
                if (m_hiddenByPage) {
                    static_cast<PaneFloatingWindow *>(m_host.data())->finishDeferredResize();
                    rememberGeometry();
                    m_returnFade->stop();
                    m_host->hide();
                    m_host->setWindowOpacity(1.0);
                }
            } else if (event->type() == QEvent::Show && m_hiddenByPage) {
                const auto generation = m_generation;
                // Let Resolve finish the page's Show handling before revealing
                // its floating windows. Do not pump a nested event loop.
                QTimer::singleShot(0, this, [this, generation]() {
                    if (!m_host || !m_root || !m_root->isVisible() || !m_hiddenByPage || m_generation != generation) return;
                    m_hiddenByPage = false;
                    if (m_host->layout()) m_host->layout()->activate();
                    applyPaneDarkCaption(m_host, m_root->window());
                    // Returning pages reuse their windows at full opacity.
                    // Cancel any initial-restoration fade still in progress.
                    m_returnFade->stop();
                    m_host->setWindowOpacity(1.0);
                    m_host->show();
                    // Reuse the existing widget tree and let Qt coalesce its
                    // pending paints. Forcing a synchronous repaint here makes
                    // every returning pane finish painting before the next can
                    // appear, producing a sequential rebuild look.
                    m_host->update();
                });
            }
        }
        if (object == m_slot && m_splitter && !m_restoring && event->type() == QEvent::Show) {
            const auto generation = m_generation;
            QTimer::singleShot(0, this, [this, generation]() {
                if (active() && m_splitter && m_slot && m_generation == generation) m_slot->hide();
            });
        }
        if (object == m_host && (event->type() == QEvent::WindowActivate || event->type() == QEvent::WindowDeactivate)) {
            QTimer::singleShot(0, this, [this]() {
                if (m_host && m_root) applyPaneDarkCaption(m_host, m_root->window());
            });
        }
        if (object == m_host && event->type() == QEvent::Close) {
            const auto error = dock();
            static_cast<QCloseEvent *>(event)->setAccepted(error.isEmpty());
            return true;
        }
        return QObject::eventFilter(object, event);
    }

private:
    void stopAuditionBridge()
    {
        delete m_auditionHeaderDrag; m_auditionHeaderDrag = nullptr;
        if (!m_auditionBridge) return;
        m_auditionBridge->stop();
        // Native event processing can dock reentrantly. Keep the stopped filter
        // alive until its synthetic notification has finished unwinding.
        m_auditionBridge->deleteLater();
        m_auditionBridge = nullptr;
    }
    void restoreInspectorTitlePolicy()
    {
        if (m_inspectorTitle) m_inspectorTitle->setSizePolicy(m_inspectorTitlePolicy);
        m_inspectorTitle=nullptr;
    }
    void addNativeContent(QWidget *widget)
    {
        if (auto grid=qobject_cast<QGridLayout *>(m_nativeLayout.data()))
            grid->addWidget(widget,m_gridRow,m_gridColumn,m_gridRows,m_gridColumns,m_contentAlignment);
        else if (auto box=qobject_cast<QBoxLayout *>(m_nativeLayout.data()))
            box->insertWidget(0,widget,m_boxStretch,m_contentAlignment);
    }
    void queueGraphRecovery()
    {
        if (m_graphRecoveryQueued || m_restoring || !m_nativeLayout) return;
        m_graphRecoveryQueued = true;
        const auto generation = m_generation;
        // Resolve's toolbar handler must finish hiding/reallocating the native
        // slot first. It may also move or hide the graph; reclaim only that
        // graph, leaving its toolbar state and the native slot untouched.
        QTimer::singleShot(0, this, [this, generation] {
            if (generation != m_generation) return;
            m_graphRecoveryQueued = false;
            recoverFloatingGraph();
        });
    }
    void recoverFloatingGraph()
    {
        if (m_restoring || m_recoveringGraph || !m_nativeLayout || !m_slot
            || m_slot->layout() != m_nativeLayout || !m_content || !m_host || !m_hostLayout
            || m_host->layout() != m_hostLayout) return;
        m_recoveringGraph = true;
        if (m_content->parentWidget() != m_host || m_hostLayout->indexOf(m_content) < 0)
            m_hostLayout->addWidget(m_content);
        if (m_content->isHidden()) m_content->show();
        m_recoveringGraph = false;
    }
    void clear()
    {
        stopAuditionBridge();
        restoreInspectorTitlePolicy();
        delete m_nodesBounds; m_nodesBounds=nullptr;
        m_geometryTimer->stop();
        m_returnFade->stop();
        m_returnFade->setTargetObject(nullptr);
        for (const auto &connection : m_nativeToggleConnections) disconnect(connection);
        m_nativeToggleConnections.clear();
        if (m_content) m_content->removeEventFilter(this);
        m_graphRecoveryQueued = false;
        m_nativeLayout = nullptr; m_hostLayout = nullptr; m_content = nullptr;
        m_hiddenByPage = false;
        m_splitter = nullptr; m_splitterSizes.clear(); m_splitterWidgets.clear();
        m_slot = nullptr; m_root = nullptr; m_host = nullptr;
        m_layout = nullptr; m_placeholderLayout = nullptr; m_placeholder = nullptr;
    }
    QPointer<QWidget> m_slot, m_root, m_host, m_placeholder;
    QPointer<QLabel> m_inspectorTitle;
    QSizePolicy m_inspectorTitlePolicy;
    NodesCanvasBounds *m_nodesBounds=nullptr;
    QPointer<AuditionActivationBridge> m_auditionBridge;
    PaneHeaderDrag *m_auditionHeaderDrag = nullptr;
    QPointer<QLayout> m_layout;
    QPointer<QLayout> m_nativeLayout;
    int m_boxStretch=0;
    QPointer<QLayout> m_hostLayout;
    QPointer<QWidget> m_content;
    QList<QMetaObject::Connection> m_nativeToggleConnections;
    bool m_graphRecoveryQueued = false;
    bool m_recoveringGraph = false;
    Qt::Alignment m_contentAlignment;
    int m_gridRow = 0, m_gridColumn = 0, m_gridRows = 1, m_gridColumns = 1;
    bool m_contentWasHidden = false;
    QPointer<QBoxLayout> m_placeholderLayout;
    QPointer<QSplitter> m_splitter;
    QList<int> m_splitterSizes;
    QList<QPointer<QWidget>> m_splitterWidgets;
    bool m_slotWasHidden = false;
    QTimer *m_geometryTimer = nullptr;
    QPropertyAnimation *m_returnFade = nullptr;
    QMetaObject::Connection m_destroyConnection;
    QByteArray m_savedGeometry; // Retained across restore/start for this pane.
    unsigned m_generation = 0;
    bool m_hiddenByPage = false;
    bool m_restoring = false;
    PaneDockingGuard m_dockingGuard{this};
};
}
#endif
