// SPDX-License-Identifier: MIT
#pragma once

#include <QApplication>
#include <QEvent>
#include <QPointer>
#include <QWidget>

namespace Unbound {
// Resolve selects a separate-window input scope before delivering WindowActivate
// to a floating pane. Restore the page scope through its normal application notify
// path, then let genuine Qt focus select the audition player. No private state.
class AuditionActivationBridge : public QObject
{
public:
    AuditionActivationBridge(QWidget *main, QWidget *page, QWidget *preview, QObject *parent = nullptr,
        QString hostName = QStringLiteral("unboundFloating_frameAudioPanelMediaPoolContainer"))
        : QObject(parent), m_main(main), m_page(page), m_preview(preview), m_hostName(std::move(hostName))
    {
        if (!main || !page || !preview || !main->isWindow() || page->window() != main) return;
        m_enabled = true;
        qApp->installEventFilter(this);
    }
    ~AuditionActivationBridge() override { qApp->removeEventFilter(this); }
    bool enabled() const { return m_enabled; }
    void stop()
    {
        m_enabled = false;
        // Docking during nested native processing must still consume our event.
        if (!m_synthetic) qApp->removeEventFilter(this);
    }
protected:
    bool eventFilter(QObject *object, QEvent *event) override
    {
        // Native application notify has seen this exact event. Do not deliver
        // fake main-window activation to QWidget or propagate it to children.
        if (event == m_synthetic && object == m_main) return true;
        if (event->type() != QEvent::WindowActivate || !m_enabled || m_synthetic) return false;
        if (!m_main || !m_page || !m_preview || !m_page->isVisible() || !m_preview->isVisible()) return false;
        auto host = m_preview->window();
        if (object != host || host == m_main || !host->isWindow() || host->isMinimized()
            || host->objectName() != m_hostName
            || QApplication::activeWindow() != host || QApplication::activeModalWidget()
            || QApplication::activePopupWidget()) return false;

        // Exactly one notification per root activation, including reactivation
        // of the same host. Child activation, clicks and focus do no work here.
        QEvent notification(QEvent::WindowActivate);
        m_synthetic = &notification;
        QPointer<AuditionActivationBridge> alive(this);
        QApplication::sendEvent(m_main, &notification);
        if (!alive) return false;
        m_synthetic = nullptr;
        if (!m_enabled) qApp->removeEventFilter(this);
        return false; // Preserve the original activation and subsequent focus.
    }
private:
    QPointer<QWidget> m_main, m_page, m_preview;
    QString m_hostName;
    QEvent *m_synthetic = nullptr;
    bool m_enabled = false;
};
}
