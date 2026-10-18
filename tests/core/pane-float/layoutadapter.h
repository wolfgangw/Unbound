// SPDX-License-Identifier: MIT
#ifndef UNBOUND_LAYOUTADAPTER_H
#define UNBOUND_LAYOUTADAPTER_H
#include <QWidget>
#include <QEventLoop>
#include <QTimer>
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
}
#endif
