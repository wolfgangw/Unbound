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
class CocoaGroupLevels;
class WindowGroupActivation : public PaneWindowGroup {
public:
    WindowGroupActivation();
    ~WindowGroupActivation() override;
    void setWindow(QWidget *window);
private:
    std::unique_ptr<CocoaGroupLevels> m_levels;
    bool m_updatingLevels=false;
    void refreshLevels();
    bool eventFilter(QObject *,QEvent *) override;
    bool raiseGroup(QWidget *selected,const QList<QWidget *> &pins);
};
bool applyPaneDarkCaption(QWidget *host, QWidget *mainWindow);
void inheritPaneTaskbarIdentity(QWidget *host, QWidget *mainWindow);
}
