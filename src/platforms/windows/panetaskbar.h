// SPDX-License-Identifier: MIT
#ifndef UNBOUND_PANETASKBAR_H
#define UNBOUND_PANETASKBAR_H
#include <QWidget>
#include <QIcon>
#include <QPalette>
#include <QGuiApplication>
#include "panewindowappearance.h"
#ifdef Q_OS_WIN
#include <qt_windows.h>
#include <shobjidl.h>
#include <shellapi.h>
#include <propkey.h>
#include <dwmapi.h>
#endif

namespace Unbound {
inline bool applyPaneDarkCaption(QWidget *host, QWidget *mainWindow)
{
#ifdef Q_OS_WIN
    if (QGuiApplication::platformName() != QLatin1String("windows")) return false;
    const auto handle = reinterpret_cast<HWND>(host->winId());
    // Page switches should hide/show the existing windows immediately. The
    // host still paints a dark background while pane contents catch up.
    const BOOL noTransitions = TRUE;
    DwmSetWindowAttribute(handle, DWMWA_TRANSITIONS_FORCEDISABLED, &noTransitions, sizeof(noTransitions));
    const BOOL dark = TRUE;
    DwmSetWindowAttribute(handle, DWMWA_USE_IMMERSIVE_DARK_MODE, &dark, sizeof(dark));
    Q_UNUSED(mainWindow);
    const COLORREF caption = RGB(PaneWindowBackgroundLevel, PaneWindowBackgroundLevel, PaneWindowBackgroundLevel);
    const COLORREF text = host->isActiveWindow() ? RGB(160, 160, 160) : RGB(96, 96, 96);
    const auto captionResult = DwmSetWindowAttribute(handle, DWMWA_CAPTION_COLOR, &caption, sizeof(caption));
    const auto textResult = DwmSetWindowAttribute(handle, DWMWA_TEXT_COLOR, &text, sizeof(text));
    return SUCCEEDED(captionResult) && SUCCEEDED(textResult);
#else
    Q_UNUSED(host);
    Q_UNUSED(mainWindow);
    return false;
#endif
}

inline void inheritPaneTaskbarIdentity(QWidget *host, QWidget *mainWindow)
{
    host->setWindowIcon(mainWindow->windowIcon());
#ifdef Q_OS_WIN
    if (QGuiApplication::platformName() != QLatin1String("windows")) return;
    // Fairlight Media Pool retains a Qt parent for Resolve's audition context.
    // That also creates a native owner, which normally suppresses its taskbar /
    // Alt+Tab entry. Opt this owned top-level host in before first show, keeping
    // both its Qt ancestry and native ownership intact.
    const auto handle = reinterpret_cast<HWND>(host->winId());
    if (GetWindow(handle, GW_OWNER))
        SetWindowLongPtr(handle, GWL_EXSTYLE, GetWindowLongPtr(handle, GWL_EXSTYLE) | WS_EX_APPWINDOW);
    // Hosts already share the process identity; copy any main-window override.
    IPropertyStore *source = nullptr;
    if (FAILED(SHGetPropertyStoreForWindow(reinterpret_cast<HWND>(mainWindow->winId()), IID_PPV_ARGS(&source)))) return;
    PROPVARIANT value;
    PropVariantInit(&value);
    if (SUCCEEDED(source->GetValue(PKEY_AppUserModel_ID, &value)) && value.vt == VT_LPWSTR && value.pwszVal && *value.pwszVal) {
        IPropertyStore *destination = nullptr;
        if (SUCCEEDED(SHGetPropertyStoreForWindow(handle, IID_PPV_ARGS(&destination)))) {
            destination->SetValue(PKEY_AppUserModel_ID, value);
            destination->Release();
        }
    }
    PropVariantClear(&value);
    source->Release();
#endif
}
}
#endif
