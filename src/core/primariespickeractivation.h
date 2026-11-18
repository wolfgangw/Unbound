// SPDX-License-Identifier: MIT
#pragma once
#include <QAbstractButton>
#include <QApplication>
#include <QMouseEvent>
#include <QPointer>
#include <QTimer>
#include <functional>

namespace Unbound {
// Resolve arms a Primaries picker in the active window. Complete its native
// button gesture after activating the window that will receive the sample.
class PrimariesPickerActivation final : public QObject {
public:
    using Viewer = std::function<QWidget *()>;
    PrimariesPickerActivation(QWidget *host, Viewer viewer)
        : QObject(host), m_host(host), m_viewer(std::move(viewer)) {
        for (auto button : host->findChildren<QAbstractButton *>()) {
            const auto name = button->objectName();
            if (name == QLatin1String("whitePickerPushBtn")
                || name == QLatin1String("blackPickerPushBtn")
                || name == QLatin1String("whiteBalancePickerPushBtn"))
                button->installEventFilter(this);
        }
    }

protected:
    bool eventFilter(QObject *object, QEvent *event) override {
        if (event->type() != QEvent::MouseButtonRelease || !available()
            || QApplication::activeWindow() != m_host)
            return false;
        auto button = static_cast<QAbstractButton *>(object);
        auto release = static_cast<QMouseEvent *>(event);
        if (button->window() != m_host || !button->isDown() || release->button() != Qt::LeftButton
            || !button->rect().contains(release->pos()))
            return false;
        QPointer<QWidget> target = m_viewer();
        if (!target || !target->isVisible() || target->isMinimized() || target == m_host)
            return false;

        // Deactivation may cancel a depressed button. Preserve the user's
        // actual release, including its hit position, and finish it only once.
        const QPointer<QAbstractButton> picker(button);
        const QPointer<PrimariesPickerActivation> alive(this);
        const QMouseEvent saved(*release);
        button->setDown(false);
        target->activateWindow();
        if (!alive) return true;
        QTimer::singleShot(0, this, [this, target, picker, saved] {
            if (!available() || !target || QApplication::activeWindow() != target
                || !picker || !picker->isVisible() || !picker->isEnabled()
                || picker->window() != m_host || m_viewer() != target)
                return;
            QMouseEvent finish(saved);
            picker->setDown(true);
            QApplication::sendEvent(picker, &finish);
        });
        return true;
    }

private:
    bool available() const {
        return !QCoreApplication::closingDown() && m_host->isVisible() && !m_host->isMinimized()
            && !QApplication::activeModalWidget() && !QApplication::activePopupWidget();
    }
    QWidget *m_host;
    Viewer m_viewer;
};
}
