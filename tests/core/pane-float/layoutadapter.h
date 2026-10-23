// SPDX-License-Identifier: MIT
#ifndef UNBOUND_LAYOUTADAPTER_H
#define UNBOUND_LAYOUTADAPTER_H
#include <QWidget>
#include <QEventLoop>
#include <QTimer>
#include <QApplication>
#include <QElapsedTimer>
#include <functional>
#include <memory>
#include <cstdio>
#include <cstdlib>
namespace LayoutTest {
inline void require(bool ok,const char *message) {
    if(!ok) { std::fprintf(stderr,"FAIL: %s\n",message); std::exit(1); }
}
inline void settle() {
    // Native activation and geometry notifications arrive asynchronously.
    QEventLoop loop; QTimer::singleShot(20,&loop,&QEventLoop::quit); loop.exec();
}
void initialize();
std::unique_ptr<QWidget> makeMainWindow();
void nativePlacementEdges();
QRect prepareMainPlacement(QWidget &window);
QRect nativeBounds(QWidget &window);
void verifyGestureDismissal(QWidget &floating,QWidget &main,QWidget &menu,const std::function<QWidget *()> &reopen);
void transientPopup(QWidget &menu);
bool ownsKeyboardFocus(QWidget &window);
void requestActivation(QWidget &window);
inline void activate(QWidget &window) {
    // Synthetic input does not perform an OS click's foreground handoff. Wait
    // for that precondition, not for the behavior under test; do not retry input.
    requestActivation(window);
    if(QGuiApplication::platformName()==QLatin1String("offscreen")) { settle(); return; }
    QElapsedTimer deadline; deadline.start();
    do {
        settle();
        if(QApplication::activeWindow()==&window && ownsKeyboardFocus(window)
            && QGuiApplication::applicationState()==Qt::ApplicationActive) return;
    } while(deadline.elapsed()<1000);
    const auto active=QApplication::activeWindow();
    std::fprintf(stderr,"Activation target=%s visible=%d nativeFocus=%d active=%s appState=%d\n",
        qPrintable(window.objectName()),window.isVisible(),ownsKeyboardFocus(window),
        active ? qPrintable(active->objectName()) : "<none>",int(QGuiApplication::applicationState()));
    require(false,"Fixture could not establish native/Qt activation before input");
}
}
#endif
