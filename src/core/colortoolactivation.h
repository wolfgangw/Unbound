// SPDX-License-Identifier: MIT
#pragma once
#include <QApplication>
#include <QAbstractButton>
#include <QPointer>
#include <QTimer>
#include <QWidget>

namespace Unbound {
// A floating tool's Qt focus is not Resolve's viewer-tool selection. Use the
// same public button action as the docked toolbar, before local picker input.
// The retained native tool container stays discoverable in its original slot.
class ColorToolActivation final : public QObject {
public:
    ColorToolActivation(QWidget *host,QWidget *page,QString selector,QObject *owner)
        :QObject(owner),m_host(host),m_page(page),m_selector(std::move(selector)) {
        discover();
        qApp->installEventFilter(this);
    }
    ~ColorToolActivation() override { stop(); }
    void stop() {
        m_stopped=true;invalidate();++m_revealGeneration;
        if(qApp)qApp->removeEventFilter(this);
    }
protected:
    bool eventFilter(QObject *object,QEvent *event) override {
        if(m_stopped || m_selecting)return false;
        const auto type=event->type();
        if(type==QEvent::ApplicationDeactivate
            || ((object==m_host || object==m_page) && type==QEvent::Hide)
            || (object==m_host && type==QEvent::WindowDeactivate)) {
            invalidate();return false;
        }
        if(type==QEvent::Show && object==m_host) {
            // Some backends deliver activation as part of showing a window,
            // even with WA_ShowWithoutActivating. Exclude the native/queued show
            // burst; genuine local input can still select immediately.
            m_revealing=true;
            const auto generation=++m_revealGeneration;
            QTimer::singleShot(0,this,[this,generation] {
                if(generation!=m_revealGeneration)return;
                QTimer::singleShot(0,this,[this,generation] {
                    if(generation==m_revealGeneration)m_revealing=false;
                });
            });
        }
        if(type==QEvent::WindowActivate && object==m_host) {
            if(m_revealing)return false;
            // Native activation/focus handlers must finish before selecting the
            // viewer tool. Show alone never selects: automatic reveals are inert.
            const auto generation=++m_generation;
            QTimer::singleShot(0,this,[this,generation] {
                if(generation==m_generation)select();
            });
        } else if(type==QEvent::MouseButtonPress && object->isWidgetType()) {
            auto widget=static_cast<QWidget *>(object);
            if(!m_host || widget->window()!=m_host)return false;
            // This press supersedes queued activation. Dock/menu gestures must
            // not change native tool selection on their way out.
            ++m_generation;
            for(auto parent=widget;parent;parent=parent->parentWidget()) {
                if(parent->objectName()==QLatin1String("resolvePaneUndockButton"))return false;
                if(parent==m_host)break;
            }
            m_revealing=false;++m_revealGeneration;
            const QPointer<QWidget> receiver(widget);
            select();
            // Native handlers may destroy the original receiver reentrantly.
            return !receiver;
        }
        return false;
    }
private:
    void invalidate() { ++m_generation;m_selected=nullptr; }
    bool eligible() const {
        return !m_stopped && !QCoreApplication::closingDown() && m_host && m_page
            && m_page->isVisible() && m_host->isVisible() && !m_host->isMinimized()
            && !m_host->property("unboundUndockDragging").toBool()
            && QApplication::activeWindow()==m_host
            && !QApplication::activeModalWidget() && !QApplication::activePopupWidget();
    }
    void discover() {
        m_buttons.clear();
        auto bottom=m_page ? m_page->findChild<QWidget *>("frameColorBottom") : nullptr;
        if(bottom)for(auto button:bottom->findChildren<QAbstractButton *>(m_selector))m_buttons.append(button);
    }
    QAbstractButton *availableButton() const {
        for(auto button:m_buttons)
            if(button && button->isVisible() && button->isEnabled())return button;
        return nullptr;
    }
    void select() {
        if(m_selecting || !eligible())return;
        auto button=availableButton();
        // Native reconstruction may replace toolbar objects. Rediscover only
        // on this deliberate activation/input, scoped to the known toolbar.
        if(!button) { discover();button=availableButton(); }
        if(!button || (m_selected==button && button->isChecked()))return;
        // Checked alone is insufficient after expanded Warper/another window
        // owns viewer input. Re-select once per activation, not per local click.
        m_selected=button;m_selecting=true;
        const QPointer<ColorToolActivation> alive(this);
        button->click();
        if(alive)m_selecting=false;
    }
    QPointer<QWidget> m_host,m_page;
    QString m_selector;
    QList<QPointer<QAbstractButton>> m_buttons;
    QPointer<QAbstractButton> m_selected;
    unsigned m_generation=0;
    unsigned m_revealGeneration=0;
    bool m_stopped=false,m_selecting=false,m_revealing=false;
};
}
