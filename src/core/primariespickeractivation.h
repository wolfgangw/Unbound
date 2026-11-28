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
        if (m_host) {
            m_host->installEventFilter(this);
            observe();
        }
    }
    ~PrimariesPickerActivation() override {
        for (const auto &widget : m_observed) if (widget) widget->removeEventFilter(this);
    }

protected:
    bool eventFilter(QObject *object, QEvent *event) override {
        if (event->type() == QEvent::ChildAdded && !m_queued) {
            m_queued = true;
            QTimer::singleShot(0, this, [this] { m_queued = false; observe(); });
            return false;
        }
        if (event->type() != QEvent::MouseButtonRelease || !available()
            || QApplication::activeWindow() != m_host->window())
            return false;
        auto button = qobject_cast<QAbstractButton *>(object);
        if (!button || !isPicker(button->objectName())) return false;
        auto release = static_cast<QMouseEvent *>(event);
        if (button->window() != m_host->window() || !button->isDown() || release->button() != Qt::LeftButton
            || !button->rect().contains(release->pos()))
            return false;
        QPointer<QWidget> target = m_viewer();
        if (!target || !target->isVisible() || target->isMinimized() || target == m_host->window())
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
                || picker->window() != m_host->window() || m_viewer() != target)
                return;
            QMouseEvent finish(saved);
            picker->setDown(true);
            QApplication::sendEvent(picker, &finish);
        });
        return true;
    }

private:
    static bool isPicker(const QString &name) {
        return name == QLatin1String("whitePickerPushBtn")
            || name == QLatin1String("blackPickerPushBtn")
            || name == QLatin1String("whiteBalancePickerPushBtn");
    }
    void observe() {
        if (!m_host) return;
        for (int i = m_observed.size() - 1; i >= 0; --i) if (!m_observed[i]) m_observed.removeAt(i);
        auto widgets = m_host->findChildren<QWidget *>();
        widgets.prepend(m_host.data());
        for (auto widget : widgets) {
            if (!m_observed.contains(widget)) {
                m_observed.append(widget);
                widget->installEventFilter(this);
            }
        }
    }
    bool available() const {
        return !QCoreApplication::closingDown() && m_host && m_host->isVisible() && !m_host->window()->isMinimized()
            && !QApplication::activeModalWidget() && !QApplication::activePopupWidget();
    }
    QPointer<QWidget> m_host;
    QList<QPointer<QWidget>> m_observed;
    bool m_queued = false;
    Viewer m_viewer;
};
}
