// SPDX-License-Identifier: MIT
#include "layouttransitionpresentation.h"
#include "layouttransition.h"
#include "layoutadapter.h"
#include "pagelayoutmenu.h"
#include <QTemporaryDir>
#include <qt_windows.h>
#include <dwmapi.h>
#include <QPushButton>
#include <QScreen>

void testLayoutCoverPresentation() {
    using namespace LayoutTest;
    QWidget window; window.resize(320,240); window.show(); activate(window);
    const auto foreground=GetForegroundWindow();
    auto cover=Unbound::makeLayoutTransitionPresentation();
    bool interrupted=false,finished=false;
    require(cover->begin(&window,[&] {interrupted=true;}),"Desktop cover capture failed");
    require(cover->retainedBytes()>0 && GetForegroundWindow()==foreground,"Cover changed focus or captured no pixels");
    cover->reveal(60,[&](bool success) {require(success,"Cover reveal failed");finished=true;});
    QElapsedTimer wait;wait.start();while(!finished && wait.elapsed()<1000)settle();
    require(finished && !interrupted && cover->retainedBytes()==0,"Cover did not finish/release capture");
    require(!FindWindowW(L"UnboundLayoutCoverExperiment",nullptr),"Cover window survived reveal");
    require(cover->begin(&window,[] {}),"Second cover capture failed");
    cover->cancel();
    require(cover->retainedBytes()==0 && !FindWindowW(L"UnboundLayoutCoverExperiment",nullptr),"Cancel retained cover resources");
    require(cover->begin(&window,[] {}),"Third cover capture failed");
    finished=false;cover->reveal(120,[&](bool) {finished=true;});cover.reset();
    wait.restart();while(wait.elapsed()<180)settle();
    require(!finished && !FindWindowW(L"UnboundLayoutCoverExperiment",nullptr),"Destruction left a callback or cover alive");
    require(GetForegroundWindow()==foreground,"Cover changed foreground activation");
    QWidget page(&window);page.setObjectName("WStackPage_Color");page.show();
    auto experiment=Unbound::LayoutTransition::begin(&window,page.objectName());
    require(experiment && FindWindowW(L"UnboundLayoutCoverExperiment",nullptr),"Core cover did not start");
    QDialog embedded(&page);embedded.setWindowFlags(Qt::Widget);embedded.show();
    QMetaObject::invokeMethod(qApp,"applicationStateChanged",Qt::DirectConnection,
        Q_ARG(Qt::ApplicationState,Qt::ApplicationInactive));
    settle();
    require(FindWindowW(L"UnboundLayoutCoverExperiment",nullptr),"Embedded dialog or Qt activation churn cancelled cover");
    QDialog realDialog(&window);realDialog.show();settle();
    require(!FindWindowW(L"UnboundLayoutCoverExperiment",nullptr),"Real dialog remained concealed by cover");
    realDialog.hide();
    window.resize(700,500);window.move(QGuiApplication::primaryScreen()->availableGeometry().topLeft()+QPoint(80,80));
    auto palette=window.palette();palette.setColor(QPalette::Window,QColor(17,73,129));
    window.setPalette(palette);window.setAutoFillBackground(true);window.show();activate(window);settle();
    QWidget popup(&window,Qt::Tool|Qt::FramelessWindowHint|Qt::WindowStaysOnTopHint);
    auto popupPalette=popup.palette();popupPalette.setColor(QPalette::Window,QColor(190,31,61));
    popup.setPalette(popupPalette);popup.setAutoFillBackground(true);popup.resize(180,140);
    popup.move(window.mapToGlobal(QPoint(30,30)));popup.show();activate(popup);settle();
    const auto popupHandle=reinterpret_cast<HWND>(popup.winId());DWORD prior=0,after=0;
    require(GetWindowDisplayAffinity(popupHandle,&prior),"Cannot inspect fixture popup capture affinity");
    const auto oldCenter=popup.geometry().center();
    cover=Unbound::makeLayoutTransitionPresentation();
    require(cover->begin(&window,[] {},{&popup}),"Cover could not preserve a live popup");
    require(GetWindowDisplayAffinity(popupHandle,&after) && prior==after,"Capture changed popup affinity permanently");
    require(WindowFromPoint(POINT{oldCenter.x(),oldCenter.y()})==popupHandle,"Cover occluded the interactive popup");
    popup.move(popup.pos()+QPoint(300,0));settle();DwmFlush();
    auto screenDc=GetDC(nullptr);auto pixel=GetPixel(screenDc,oldCenter.x(),oldCenter.y());ReleaseDC(nullptr,screenDc);
    require(pixel==RGB(17,73,129),"Snapshot retained a frozen popup or black capture placeholder");
    const auto center=popup.geometry().center();
    require(WindowFromPoint(POINT{center.x(),center.y()})==popupHandle,"Moved popup lost input above the cover");
    cover->cancel();popup.hide();
    QTemporaryDir settingsDirectory;Unbound::PaneSettings settings(settingsDirectory.filePath("menu.json"));
    Unbound::PageLayoutMenu menu(settings,"WStackPage_Color",&window);
    activate(window);menu.popup(window.mapToGlobal(QPoint(30,30)));activate(menu);
    const auto menuHandle=reinterpret_cast<HWND>(menu.winId());
    require(GetWindowLongPtr(menuHandle,GWL_EXSTYLE)&WS_EX_LAYERED,"Production menu fixture is not translucent");
    GetWindowDisplayAffinity(menuHandle,&prior);
    require(cover->begin(&window,[] {},{&menu}),"Production translucent menu cannot remain live above capture");
    GetWindowDisplayAffinity(menuHandle,&after);const auto menuCenter=menu.geometry().center();
    require(prior==after && WindowFromPoint(POINT{menuCenter.x(),menuCenter.y()})==menuHandle
        && GetForegroundWindow()==menuHandle,"Production menu lost capture affinity, input or focus");
    cover->cancel();menu.dismiss();
    std::puts("PASS: native layout cover capture, reveal, cancellation and release");
}
