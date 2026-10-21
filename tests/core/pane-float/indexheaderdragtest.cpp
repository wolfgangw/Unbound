// SPDX-License-Identifier: MIT
#include "panedecoration.h"
#include "layoutadapter.h"
#include <QFrame>
#include <QPushButton>
#include <QVBoxLayout>
#include <cstdio>

void testIndexHeaderDrag() {
    if(QGuiApplication::platformName()==QLatin1String("offscreen")) return;
    using LayoutTest::require;
    Unbound::PaneFloatingWindow host;
    host.setGeometry(300,200,600,300);
    auto outer=new QVBoxLayout(&host);
    auto pane=new QFrame; pane->setObjectName("frameEditPageEditIndex"); outer->addWidget(pane);
    auto column=new QVBoxLayout(pane);
    auto tabs=new QFrame; tabs->setObjectName("frameEditPageIndexTabContainer"); column->addWidget(tabs);
    auto row=new QHBoxLayout(tabs); row->addStretch();
    int clicks=0;
    QList<QPushButton *> buttons;
    for(const auto &name:{"Edit Index","Tracks","Markers"}) {
        auto button=new QPushButton(name); row->addWidget(button); buttons.append(button);
        QObject::connect(button,&QPushButton::clicked,[&] { ++clicks; });
    }
    row->addStretch();
    auto header=new QFrame; header->setObjectName("frameEDLTopBar"); column->addWidget(header);
    auto actions=new QHBoxLayout(header); actions->addStretch();
    auto options=new QToolButton; options->setObjectName("btnEdlOptions"); actions->addWidget(options);
    column->addStretch();
    Unbound::PaneDecoration decoration(pane,&host,[] {});
    decoration.setFloating(true);
    host.show(); LayoutTest::settle();
    require(decoration.button()->parentWidget()==header,"Index button moved out of native action header");
    auto mouse=[](QWidget *target,QEvent::Type type,QPoint point,Qt::MouseButton button,Qt::MouseButtons held) {
        QMouseEvent event(type,point,target->mapToGlobal(point),button,held,Qt::NoModifier);
        QApplication::sendEvent(target,&event);
    };
    auto gesture=[&](QWidget *surface,bool expected) {
        const QPoint start(3,surface->height()/2);
        mouse(surface,QEvent::MouseButtonPress,start,Qt::LeftButton,Qt::LeftButton);
        mouse(surface,QEvent::MouseMove,start+QPoint(QApplication::startDragDistance()+1,0),Qt::NoButton,Qt::LeftButton);
        require(host.interactiveDragActive()==expected,"Index upper-row drag state incorrect");
        mouse(surface,QEvent::MouseButtonRelease,start,Qt::LeftButton,Qt::NoButton);
        require(!host.interactiveDragActive(),"Index upper-row release retained drag");
    };
    gesture(tabs,true);
    gesture(header,true);
    for(auto button:buttons) gesture(button,false);
    require(clicks==3,"Index tabs lost native click behavior");
    // The same retained row must be inert as a drag surface after docking.
    QWidget docked;
    pane->setParent(&docked); pane->show(); docked.show(); LayoutTest::settle();
    gesture(tabs,false);
    pane->setParent(&host); outer->addWidget(pane); pane->show(); LayoutTest::settle();
    gesture(tabs,true);
    std::puts("PASS: Index upper-row and action-header dragging, native tab clicks and dock/refloat lifecycle");
}
