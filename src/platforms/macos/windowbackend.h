// SPDX-License-Identifier: MIT
#pragma once
#include "panefloatingwindow.h"
#include <QObject>
#include <QPointer>
namespace Unbound {
class MainWindowSnap {
public:
    MainWindowSnap();
    ~MainWindowSnap();
    void setWindow(QWidget *window);
private:
    std::unique_ptr<MacWindowInteraction> m_interaction;
};
class WindowGroupActivation : public QObject {
public:
    WindowGroupActivation();
    ~WindowGroupActivation() override;
    void setWindow(QWidget *window) { m_main = window; }
protected:
    bool eventFilter(QObject *object, QEvent *event) override;
private:
    void raiseGroup();
    QPointer<QWidget> m_main, m_selected;
    bool m_pending = false, m_raising = false;
};
bool applyPaneDarkCaption(QWidget *host, QWidget *mainWindow);
void inheritPaneTaskbarIdentity(QWidget *host, QWidget *mainWindow);
}
