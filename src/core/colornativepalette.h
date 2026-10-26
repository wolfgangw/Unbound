// SPDX-License-Identifier: MIT
#pragma once
#include <QAbstractButton>
#include <QDialog>
#include <QKeyEvent>
#include <QPointer>
#include <QTimer>
#include <QVBoxLayout>
#include <functional>
#include "panecatalogue.h"
#ifdef UNBOUND_PALETTE_SIZE_DIAGNOSTICS
#include "palettesizediagnostics.h"
#endif

namespace Unbound {
// Resolve supplies histogram/image data to registered expanded palettes even
// when another tool is selected. Keep that registration and the entire native
// dialog; moving just the palette loses the native update subscription.
class ColorNativePalette final : public QObject {
public:
    explicit ColorNativePalette(QObject *owner) : QObject(owner) {}
    ~ColorNativePalette() override { unwatch(); }
    std::function<void()> dockRequested;
    static bool supports(QWidget *slot) {
        return slot && (slot->objectName()==QLatin1String("curvesTab")
            || slot->objectName()==QLatin1String("colorMapperTab"));
    }
    static QWidget *palette(QWidget *slot) {
        if(!supports(slot)) return nullptr;
        const auto control=PaneCatalogue::colorControl(slot->objectName());
        if(auto content=slot->findChild<QWidget *>(control->headerOwner)) return content;
        // Already expanded natively: its dialog is owned by this tool family.
        for(auto scope=slot->parentWidget();scope;scope=scope->parentWidget()) {
            if(scope->objectName()!=control->scope) continue;
            for(auto dialog:scope->findChildren<QWidget *>("UiFloatingPaletteWidgetDialog"))
                if(auto content=dialog->findChild<QWidget *>(control->headerOwner)) return content;
            break;
        }
        return nullptr;
    }
    QString start(QWidget *slot,QWidget *host,QWidget *page) {
        m_slot=slot; m_palette=palette(slot); m_host=host; m_page=page;
        if(!m_palette) return QStringLiteral("The native Color palette is unavailable; no changes made.");
        auto dialog=m_palette->window();
        if(dialog->objectName()!=QLatin1String("UiFloatingPaletteWidgetDialog")) {
            auto expand=m_palette->findChild<QAbstractButton *>("btnUndock");
            if(!expand || !expand->isEnabled())
                return QStringLiteral("The native Color palette cannot be expanded right now.");
            expand->click();
            if(!m_palette) return QStringLiteral("DaVinci Resolve replaced the Color palette during expansion.");
            dialog=m_palette->window();
        }
        if(dialog->objectName()!=QLatin1String("UiFloatingPaletteWidgetDialog") || !qobject_cast<QDialog *>(dialog))
            return QStringLiteral("DaVinci Resolve did not provide the expected expanded Color palette.");
        m_dialog=qobject_cast<QDialog *>(dialog); m_parent=dialog->parentWidget();
        m_flags=dialog->windowFlags(); m_geometry=dialog->geometry();
        m_showWithoutActivating=dialog->testAttribute(Qt::WA_ShowWithoutActivating);
        dialog->setAttribute(Qt::WA_ShowWithoutActivating);
        embed();
        dialog->installEventFilter(this); m_palette->installEventFilter(this);
#ifdef UNBOUND_PALETTE_SIZE_DIAGNOSTICS
        recordPaletteSizes(host,slot->objectName());
#endif
        return {};
    }
    bool intact() const {
        return m_host && m_palette && m_slot
            && ((m_dialog && m_dialog->parentWidget()==m_host && m_dialog->isAncestorOf(m_palette))
                || m_slot->isAncestorOf(m_palette));
    }
    bool stop() {
        if(!intact()) return false;
        m_stopping=true; unwatch();
        if(m_dialog) {
            m_host->layout()->removeWidget(m_dialog);
            m_dialog->setParent(m_parent,m_flags);
            m_dialog->setGeometry(m_geometry);
            m_dialog->setAttribute(Qt::WA_ShowWithoutActivating,m_showWithoutActivating);
            // Reparenting hides the dialog. QDialog::closeEvent does not call
            // reject while hidden, so close() can succeed without returning
            // anything. Invoke the public virtual return path directly instead.
            // Resolve owns palette reinsertion and expanded registration cleanup.
            if(!m_slot->isAncestorOf(m_palette)) m_dialog->reject();
        }
        if(m_palette && m_slot && m_slot->isAncestorOf(m_palette)) return true;
        // A rejected native close must leave a usable floating pane.
        if(m_dialog) {
            embed(); m_dialog->installEventFilter(this);
            if(m_palette) m_palette->installEventFilter(this);
        }
        m_stopping=false;
        return false;
    }
protected:
    bool eventFilter(QObject *object,QEvent *event) override {
        if(m_stopping) return false;
        if(object==m_dialog && (event->type()==QEvent::Close
            || (event->type()==QEvent::KeyPress && static_cast<QKeyEvent *>(event)->key()==Qt::Key_Escape))) {
            // Unwind the native event before session teardown/reparenting.
            event->ignore(); queueCheck(true); return true;
        }
        if(event->type()==QEvent::Hide || event->type()==QEvent::ParentChange) queueCheck(false);
        return false;
    }
private:
    void embed() {
        m_dialog->setParent(m_host,Qt::Widget);
        if(!m_host->layout()) {
            auto layout=new QVBoxLayout(m_host); layout->setContentsMargins(0,0,0,0);
        }
        m_host->layout()->addWidget(m_dialog); m_dialog->show();
    }
    void unwatch() {
        if(m_dialog) m_dialog->removeEventFilter(this);
        if(m_palette) m_palette->removeEventFilter(this);
    }
    void queueCheck(bool dock) {
        m_dockQueued=m_dockQueued || dock;
        if(m_queued) return;
        m_queued=true;
        QTimer::singleShot(0,this,[this] {
            m_queued=false;
            if(m_stopping || !m_palette || !m_slot) return;
            if(m_dockQueued || m_slot->isAncestorOf(m_palette)) {
                m_dockQueued=false;
                const auto request=dockRequested; // Docking can destroy this adapter.
                if(request) request();
            } else if(m_dialog && m_host && m_page && m_page->isVisible()
                && m_dialog->parentWidget()==m_host && m_dialog->isHidden()) m_dialog->show();
        });
    }
    QPointer<QWidget> m_slot,m_palette,m_parent,m_host,m_page;
    QPointer<QDialog> m_dialog;
    Qt::WindowFlags m_flags;
    QRect m_geometry;
    bool m_showWithoutActivating=false,m_stopping=false,m_queued=false,m_dockQueued=false;
};
}
