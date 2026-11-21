// SPDX-License-Identifier: MIT
#pragma once
#include <QApplication>
#include <QPointer>
#include <QScreen>
#include <QTimer>
#include <QWidget>

namespace Unbound {
inline QWidget *visibleCinemaForPage(QWidget *page) {
    if (!page || page->isHidden() || page->window()->isMinimized()
        || !page->isVisibleTo(page->window())) return nullptr;
    for (auto window : QApplication::topLevelWidgets()) {
        if (window->isVisible() && window->inherits("UiCinemaViewerDialog")) return window;
    }
    return nullptr;
}

inline bool paneOutsideCinemaDisplay(QWidget *host, QWidget *cinema) {
    if (!host || !cinema || !host->isVisible() || host->isMinimized()) return false;
    auto screen = cinema->screen();
    return screen && !screen->geometry().intersects(host->frameGeometry())
        && QGuiApplication::screenAt(host->frameGeometry().center());
}

inline bool cinemaKeepsPane(QWidget *page, QWidget *host) {
    return page && !page->window()->isVisible() && host && host->isVisible()
        && paneOutsideCinemaDisplay(host, visibleCinemaForPage(page));
}

// Resolve can hide the workspace before showing Cinema. Defer only this
// inherited hide, and only for an existing float on a multi-display desktop.
template<class Hide>
void onPanePageHide(QObject *session, QWidget *page, QWidget *host, Hide hide) {
    if (!host || !host->isVisible() || !page || page->isHidden()
        || page->window()->isVisible() || page->window()->isMinimized()
        || QApplication::screens().size() < 2) {
        hide();
        return;
    }
    QTimer::singleShot(0, session, [page=QPointer<QWidget>(page), host=QPointer<QWidget>(host), hide] {
        if (page && host && !page->isVisible() && !cinemaKeepsPane(page,host))
            hide();
    });
}
}
