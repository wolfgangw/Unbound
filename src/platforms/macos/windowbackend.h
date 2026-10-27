// SPDX-License-Identifier: MIT
#pragma once
#include "panefloatingwindow.h"
#include <QObject>
#include <QPointer>
#include "panewindowgroup.h"
namespace Unbound {
class MainWindowSnap {
public:
    MainWindowSnap();
    ~MainWindowSnap();
    void setWindow(QWidget *window);
private:
    std::unique_ptr<MacWindowInteraction> m_interaction;
};
class WindowGroupActivation : public PaneWindowGroup {
public:
    WindowGroupActivation();
private:
    bool raiseGroup(QWidget *selected,const QList<QWidget *> &pins);
};
bool applyPaneDarkCaption(QWidget *host, QWidget *mainWindow);
void inheritPaneTaskbarIdentity(QWidget *host, QWidget *mainWindow);
}
