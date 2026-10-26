// SPDX-License-Identifier: MIT
#include "panedecoration.h"
#include "sidebarfloatsession.h"
#include "panesettings.h"
#include "layoutadapter.h"
#include <QFrame>
#include <QPushButton>
#include <QTemporaryDir>
#include <QStackedWidget>
#include <cstdio>

void testColorViewer() {
    using namespace Unbound;
    using LayoutTest::require;
    QWidget page; page.setObjectName("WStackPage_Color"); page.resize(800,600);
    auto outer=new QVBoxLayout(&page);
    auto container=new QFrame; container->setObjectName("m_pFramePlayerContainer"); outer->addWidget(container);
    auto allocation=new QHBoxLayout(container);
    auto slot=new QFrame; slot->setObjectName("m_pFramePlayer"); allocation->addWidget(slot);
    auto native=new QGridLayout(slot);
    auto content=new QFrame; content->setObjectName("UiMainViewerForm"); native->addWidget(content,0,0);
    auto column=new QVBoxLayout(content);
    auto header=new QFrame; header->setObjectName("framePlayerStatusPrimary"); column->addWidget(header);
    auto row=new QHBoxLayout(header);
    auto title=new QLabel("Timeline 1"); row->addWidget(title);
    auto actions=new QFrame; actions->setObjectName("framePlayerTitleRight"); row->addWidget(actions);
    auto actionRow=new QHBoxLayout(actions);
    auto options=new QToolButton; options->setObjectName("btnOptions"); actionRow->addWidget(options);
    auto tools=new QStackedWidget; tools->setObjectName("m_StackedWidgetSubOptions"); column->addWidget(tools);
    auto toolPage=new QWidget; tools->addWidget(toolPage);
    auto toolRow=new QHBoxLayout(toolPage); toolRow->addStretch();
    auto toolButton=new QPushButton("Wipe"); toolRow->addWidget(toolButton);
    column->addWidget(new QLabel("Native image surface"),1);
    auto play=new QPushButton("Play"); column->addWidget(play);
    int plays=0; QObject::connect(play,&QPushButton::clicked,[&]{++plays;});
    PaneDecoration decoration(slot,&page,[]{});
    page.show(); LayoutTest::settle();
    require(decoration.usesNativeHeader() && actionRow->indexOf(decoration.button())==actionRow->indexOf(options)-1,
        "Color viewer button must precede native options");
    SidebarFloatSession session;
    require(session.start(slot,"Viewer",&page,false,false,true,false).isEmpty(),"Color viewer transfer failed");
    decoration.setFloating(true); LayoutTest::settle();
    require(slot->parentWidget()==container && slot->layout()==native && allocation->indexOf(slot)>=0,
        "Color viewer changed the outer native/Clips adapter allocation");
    require(content->parentWidget()==session.floatingWindow() && session.floatingWindow()->isVisible(),
        "Color viewer did not transfer the complete form");
    if(QGuiApplication::platformName()!=QLatin1String("offscreen")) {
        auto host=static_cast<PaneFloatingWindow *>(session.floatingWindow());
        auto gesture=[&](QWidget *surface,bool expected) {
            const QPoint start(2,2);
            auto send=[&](QEvent::Type type,QPoint point,Qt::MouseButton button,Qt::MouseButtons held) {
                QMouseEvent event(type,point,surface->mapToGlobal(point),button,held,Qt::NoModifier);
                QApplication::sendEvent(surface,&event);
            };
            send(QEvent::MouseButtonPress,start,Qt::LeftButton,Qt::LeftButton);
            send(QEvent::MouseMove,start+QPoint(QApplication::startDragDistance()+1,0),Qt::NoButton,Qt::LeftButton);
            require(host->interactiveDragActive()==expected,"Color viewer header drag routing incorrect");
            send(QEvent::MouseButtonRelease,start,Qt::LeftButton,Qt::NoButton);
            require(!host->interactiveDragActive(),"Color viewer header retained drag after release");
        };
        gesture(header,true); gesture(title,true); gesture(toolPage,true);
        gesture(options,false); gesture(toolButton,false);
    }
    play->click(); require(plays==1,"Color viewer transport connection lost");
    page.hide(); LayoutTest::settle(); require(!session.floatingWindow()->isVisible(),"Color viewer survived page hide");
    page.show(); LayoutTest::settle(); require(session.floatingWindow()->isVisible(),"Color viewer failed page return");
    session.floatingWindow()->resize(620,420); LayoutTest::settle();
    QTemporaryDir dir; PaneSettings settings(dir.filePath("settings.json"));
    require(settings.saveFloating("Color/m_pFramePlayer",true).isEmpty(),"Color viewer floating persistence failed");
    require(settings.save("Color/m_pFramePlayer",session.floatingWindow()->saveGeometry()).isEmpty(),"Color viewer geometry persistence failed");
    PaneSettings reloaded(dir.filePath("settings.json"));
    require(reloaded.floating("Color/m_pFramePlayer"),"Color viewer missing after settings reload");
    require(session.dock().isEmpty(),"Color viewer docking failed"); LayoutTest::settle();
    require(!session.active() && content->parentWidget()==slot && native->itemAtPosition(0,0)->widget()==content,
        "Color viewer native grid slot not restored");
    play->click(); require(plays==2,"Color viewer transport lost after docking");
    std::puts("PASS: Color viewer native header, full-form transfer, outer allocation, page retention, settings and docking");
}
