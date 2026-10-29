// SPDX-License-Identifier: MIT
#pragma once
#include <QAbstractButton>
#include <QPointer>
#include <QVariant>

namespace Unbound {
// Preserve the user's checked state while native initialization temporarily
// changes a toggle. The owner still performs and reverses the native clicks.
// This guard spans deferred work without blocking signals or native handlers.
class NativeToggleState final {
public:
    NativeToggleState()=default;
    NativeToggleState(const NativeToggleState &)=delete;
    NativeToggleState &operator=(const NativeToggleState &)=delete;
    ~NativeToggleState() { end(); }
    static bool temporary(QAbstractButton *button) {
        return button && button->property(property()).isValid();
    }
    static bool checked(QAbstractButton *button) {
        return temporary(button) ? button->property(property()).toBool() : button->isChecked();
    }
    void begin(QAbstractButton *button) {
        end(); if(!button)return;
        m_button=button; m_previous=button->property(property());
        button->setProperty(property(),checked(button));
    }
    void end() {
        if(m_button)m_button->setProperty(property(),m_previous);
        m_button=nullptr;m_previous={};
    }
private:
    static const char *property() {return "unboundNativeToggleUserState";}
    QPointer<QAbstractButton> m_button;
    QVariant m_previous;
};
}
