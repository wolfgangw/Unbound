// SPDX-License-Identifier: MIT
#include "windowplacement.h"
#include <QWidget>
#include <QGuiApplication>
#include <QJsonDocument>
#include <qt_windows.h>
#include "panegridreferences.h"
#include "panesnapwindows.h"
namespace Unbound { namespace Platform {
bool restoreWindowPlacement(QWidget *window, const QJsonObject &document, const QJsonArray &screens)
{
    if (document.value("format").toString()==QLatin1String("unbound-main-placement-1") && QGuiApplication::platformName()==QLatin1String("windows")
        && document.value("screens").toArray()==screens) {
        const auto rect=document.value("normal").toArray();
        if (rect.size()==4 && rect[2].toInt()>rect[0].toInt() && rect[3].toInt()>rect[1].toInt()) {
            WINDOWPLACEMENT placement{}; placement.length=sizeof(placement);
            const auto handle=reinterpret_cast<HWND>(window->winId());
            if (GetWindowPlacement(handle,&placement)) {
                placement.flags=0;
                placement.showCmd=document.value("maximized").toBool() ? SW_SHOWMAXIMIZED : SW_SHOWNOACTIVATE;
                placement.rcNormalPosition=RECT{rect[0].toInt(),rect[1].toInt(),rect[2].toInt(),rect[3].toInt()};
                return SetWindowPlacement(handle,&placement);
            }
        }
    }
    return false;
}
QByteArray saveWindowPlacement(QWidget *window, const QJsonArray &screens)
{
    const auto qt=window->saveGeometry();
    if (QGuiApplication::platformName()==QLatin1String("windows")) {
        WINDOWPLACEMENT placement{}; placement.length=sizeof(placement);
        if (GetWindowPlacement(reinterpret_cast<HWND>(window->winId()),&placement)) {
            // Resolve's custom frame can disagree with Qt's cached margins.
            // Persist and restore the same native workspace coordinates;
            // never reconstruct this rectangle from QWidget frame margins.
            const auto &r=placement.rcNormalPosition;
            return QJsonDocument(QJsonObject{{"format","unbound-main-placement-1"},
                {"qt",QString::fromLatin1(qt.toBase64())},{"screens",screens},
                {"normal",QJsonArray{r.left,r.top,r.right,r.bottom}},
                {"maximized",IsZoomed(reinterpret_cast<HWND>(window->winId()))!=FALSE}}).toJson(QJsonDocument::Compact);
        }
    }
    return qt;
}
bool windowPlacementEligible(QWidget *window)
{
    if (QGuiApplication::platformName()==QLatin1String("windows")
        && !(GetWindowLongPtr(reinterpret_cast<HWND>(window->winId()),GWL_STYLE)&WS_CAPTION)) return false;
    return true;
}
QRect windowPlacementReference(QWidget *window,const QJsonObject &saved)
{
    QList<QRect> areas;for(auto screen:QGuiApplication::screens())areas.append(screen->availableGeometry());
    const auto fallback=gridSavedRect(saved,areas);
    if(!window || QGuiApplication::platformName()!=QLatin1String("windows"))return fallback;
    QRect normal;
    if(!readRectangle(saved.value("normalRect"),normal))return fallback;
    // Use native placement only for an unchanged desktop, as restoration does.
    QJsonArray monitors;
    for(auto screen:QGuiApplication::screens())monitors.append(QJsonObject{
        {"name",screen->name()},{"workArea",rectangleJson(screen->availableGeometry())},{"scaleFactor",screen->devicePixelRatio()}});
    if(monitors!=saved.value("monitors").toArray())return fallback;
    RECT raw{normal.x(),normal.y(),normal.x()+normal.width(),normal.y()+normal.height()};
    // WINDOWPLACEMENT stores workspace coordinates relative to the monitor's work
    // area offset. Match the monitor using each monitor's own workspace-to-screen
    // shift so top/left taskbars near an adjacent monitor do not pick the neighbor.
    struct Match { RECT raw; MONITORINFOEXW info{}; qint64 overlap=-1; bool found=false; } match{raw};
    EnumDisplayMonitors(nullptr,nullptr,[](HMONITOR monitor,HDC,LPRECT,LPARAM data)->BOOL {
        auto &m=*reinterpret_cast<Match *>(data);
        MONITORINFOEXW candidate{};candidate.cbSize=sizeof(candidate);
        if(!GetMonitorInfoW(monitor,&candidate))return TRUE;
        RECT shifted=m.raw;
        OffsetRect(&shifted,candidate.rcWork.left-candidate.rcMonitor.left,candidate.rcWork.top-candidate.rcMonitor.top);
        RECT inter{};
        const qint64 area=IntersectRect(&inter,&shifted,&candidate.rcMonitor)
            ? qint64(inter.right-inter.left)*qint64(inter.bottom-inter.top) : 0;
        if(area>m.overlap) { m.overlap=area; m.info=candidate; m.found=true; }
        return TRUE;
    },reinterpret_cast<LPARAM>(&match));
    MONITORINFOEXW info{};info.cbSize=sizeof(info);
    if(match.found && match.overlap>0) info=match.info;
    else if(!GetMonitorInfoW(MonitorFromRect(&raw,MONITOR_DEFAULTTONEAREST),&info))return fallback;
    QScreen *screen=nullptr;
    for(auto item:QGuiApplication::screens())if(item->name()==QString::fromWCharArray(info.szDevice)) {screen=item;break;}
    if(!screen)return fallback;
    if(saved.value("maximized").toBool())return screen->availableGeometry();
    const auto handle=reinterpret_cast<HWND>(window->effectiveWinId());
    RECT current{};
    if(window->isMaximized() || window->isFullScreen() || !GetWindowRect(handle,&current))return fallback;
    const auto contact=paneContactBounds(handle);
    const double scale=screen->devicePixelRatio();
    const UINT windowDpi=GetDpiForWindow(handle);
    const double ratio=scale/(double(windowDpi ? windowDpi : 96)/96.);
    const int left=qRound((contact.x()-current.left)*ratio),top=qRound((contact.y()-current.top)*ratio);
    const int right=qRound((current.right-contact.x()-contact.width())*ratio);
    const int bottom=qRound((current.bottom-contact.y()-contact.height())*ratio);
    // WINDOWPLACEMENT stores workspace coordinates, not screen coordinates.
    OffsetRect(&raw,info.rcWork.left-info.rcMonitor.left,info.rcWork.top-info.rcMonitor.top);
    const QRect result(screen->geometry().x()+qRound((raw.left+left-info.rcMonitor.left)/scale),
        screen->geometry().y()+qRound((raw.top+top-info.rcMonitor.top)/scale),
        qRound((raw.right-raw.left-left-right)/scale),qRound((raw.bottom-raw.top-top-bottom)/scale));
    return reachableWindowGeometry(result,areas);
}
} }
