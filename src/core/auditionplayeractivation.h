// SPDX-License-Identifier: MIT
#pragma once
#include <QApplication>
#include <QPointer>
#include <QTimer>
#include <QWidget>

namespace Unbound {
// Sound Library transport routing and native audio-player activation are
// separate. Reproduce its docked preview-focus visit, preserving the control
// selected by Qt/user input. No transport commands or native player APIs.
class AuditionPlayerActivation final : public QObject {
public:
    AuditionPlayerActivation(QWidget *host,QWidget *page,QWidget *preview,QObject *parent)
        : QObject(parent),m_host(host),m_page(page),m_preview(preview) {
        if(host) host->installEventFilter(this);
    }
protected:
    bool eventFilter(QObject *object,QEvent *event) override {
        if(object!=m_host) return false;
        if(event->type()==QEvent::WindowDeactivate || event->type()==QEvent::Hide) ++m_generation;
        if(event->type()==QEvent::WindowActivate) {
            const auto generation=++m_generation;
            QTimer::singleShot(0,this,[this,generation] {
                if(generation!=m_generation || !available()) return;
                const QPointer<QWidget> focus=QApplication::focusWidget();
                const auto host=m_host,preview=m_preview;
                if(!focus || focus->window()!=host || focus==preview || preview->isAncestorOf(focus)) return;
                QPointer<AuditionPlayerActivation> alive(this);
                preview->setFocus(Qt::ActiveWindowFocusReason);
                // Native focus handlers may dock, destroy content, open a
                // dialog or select another control. Never override that choice.
                if(alive && generation==m_generation && available() && focus
                    && focus->isVisible() && focus->isEnabled() && focus->window()==host
                    && QApplication::focusWidget()==preview)
                    focus->setFocus(Qt::ActiveWindowFocusReason);
            });
        }
        return false;
    }
private:
    bool available() const {
        return m_host && m_page && m_preview && m_host->isVisible() && !m_host->isMinimized()
            && m_page->isVisible() && m_preview->isVisible() && m_preview->window()==m_host
            && QApplication::activeWindow()==m_host && !QApplication::activeModalWidget()
            && !QApplication::activePopupWidget() && !QCoreApplication::closingDown();
    }
    QPointer<QWidget> m_host,m_page,m_preview;
    quint64 m_generation=0;
};
}
