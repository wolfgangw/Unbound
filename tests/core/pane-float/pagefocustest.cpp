// SPDX-License-Identifier: MIT
#include "pagefocus.h"
#include "windowintegration.h"
#include "layoutadapter.h"
#include <QLineEdit>
#include <QVBoxLayout>
#include <QDialog>
#include <QMouseEvent>
#include <cstdio>
using LayoutTest::require;
using LayoutTest::settle;

void testPageFocus() {
    if(QGuiApplication::platformName()==QLatin1String("offscreen")) {
        std::puts("SKIP: page focus requires the native window-system backend"); return;
    }
    QWidget main;
    auto layout=new QVBoxLayout(&main);
    auto stack=new QStackedWidget; stack->setObjectName("widgetStack_Panel"); layout->addWidget(stack);
    auto edit=new QWidget; edit->setObjectName("WStackPage_Conform"); stack->addWidget(edit);
    auto audio=new QWidget; audio->setObjectName("WStackPage_Audio"); stack->addWidget(audio);
    auto editLayout=new QVBoxLayout(edit); auto editText=new QLineEdit; editLayout->addWidget(editText);
    auto audioLayout=new QVBoxLayout(audio); auto timeline=new QLineEdit; audioLayout->addWidget(timeline);
    Unbound::PaneFloatingWindow effects, viewer;
    main.setObjectName("focusMain"); effects.setObjectName("focusEffects"); viewer.setObjectName("focusViewer");
    effects.setProperty("unboundPage",audio->objectName()); viewer.setProperty("unboundPage",edit->objectName());
    auto effectsLayout=new QVBoxLayout(&effects); auto search=new QLineEdit; effectsLayout->addWidget(search);
    auto viewerLayout=new QVBoxLayout(&viewer); auto timecode=new QLineEdit; viewerLayout->addWidget(timecode);
    // Taskbar setup forces native creation before the first show in production.
    effects.winId(); viewer.winId();
    main.show(); main.activateWindow(); editText->setFocus(); settle();
    Unbound::PageFocus memory; memory.attach(&main);
    QObject::connect(stack,&QStackedWidget::currentChanged,&main,[&] {
        // Resolve selects main before hiding its outgoing floating windows.
        // Hiding the active unowned fixture first can activate another app.
        main.activateWindow();
        effects.hide(); viewer.hide();
        // Model Resolve choosing its native control and Unbound queuing reveals.
        (stack->currentWidget()==audio ? timeline : editText)->setFocus();
        QTimer::singleShot(0,&main,[&] {
            if(stack->currentWidget()==audio) effects.show(); else viewer.show();
        });
    });
    auto focus=[&](QWidget &window,QWidget *control) {
        LayoutTest::activate(window); control->setFocus(); settle();
        require(QApplication::activeWindow()==&window && QApplication::focusWidget()==control,"Focus fixture activation failed");
    };
    auto change=[&](QWidget *page) { stack->setCurrentWidget(page); settle(); settle(); };
    viewer.show(); settle();
    require(QApplication::activeWindow()==&main,"Floating host show stole native main activation");
    focus(main,editText);
    change(audio);
    require(QApplication::activeWindow()==&main && QApplication::focusWidget()==timeline,"First page visit did not preserve native main focus");
    focus(effects,search);
    change(edit);
    require(QApplication::activeWindow()==&main && QApplication::focusWidget()==editText,"Main control was not recalled");
    focus(viewer,timecode);
    // Clicking Resolve's page selector activates main before changing pages.
    // Navigation itself must not replace the remembered floating editor.
    QMouseEvent navigationPress(QEvent::MouseButtonPress,QPointF(1,1),Qt::LeftButton,Qt::LeftButton,Qt::NoModifier);
    QApplication::sendEvent(&main,&navigationPress);
    main.activateWindow(); editText->setFocus(); settle();
    stack->setCurrentWidget(audio);
    QMouseEvent navigationRelease(QEvent::MouseButtonRelease,QPointF(1,1),Qt::LeftButton,Qt::NoButton,Qt::NoModifier);
    QApplication::sendEvent(&main,&navigationRelease); settle(); settle();
    require(QApplication::activeWindow()==&effects && QApplication::focusWidget()==search,"Floating panel/control was not recalled");
    change(edit);
    require(QApplication::activeWindow()==&viewer && QApplication::focusWidget()==timecode,"Viewer focus was not recalled independently");
    change(audio); focus(main,timeline); change(edit); change(audio);
    require(QApplication::activeWindow()==&main && QApplication::focusWidget()==timeline,"Returning floating panels stole remembered timeline focus");
    focus(effects,search); change(edit);
    // A retained host may be unavailable after a layout change. Do not choose a
    // different floating window, and do not focus content reparented into main.
    effects.setProperty("unboundPage",QString()); change(audio);
    require(QApplication::activeWindow()==&main && QApplication::focusWidget()==timeline,"Unavailable target did not fall back to native main focus");
    effects.setProperty("unboundPage",audio->objectName()); focus(effects,search); change(edit);
    stack->setCurrentWidget(audio);
    QMouseEvent click(QEvent::MouseButtonPress,QPointF(1,1),Qt::LeftButton,Qt::LeftButton,Qt::NoModifier);
    QApplication::sendEvent(timeline,&click); focus(main,timeline); settle();
    require(QApplication::activeWindow()==&main,"Deferred recall overrode a newer click");
    QApplication::sendEvent(timeline,&navigationRelease); settle();
    focus(effects,search); change(edit); stack->setCurrentWidget(audio);
    QEvent deactivate(QEvent::ApplicationDeactivate); QApplication::sendEvent(qApp,&deactivate);
    settle(); require(QApplication::activeWindow()==&main,"Recall survived application deactivation");
    focus(effects,search); change(edit); stack->setCurrentWidget(audio);
    QDialog dialog(&main); dialog.setModal(true); dialog.show(); dialog.activateWindow(); settle();
    require(QApplication::activeModalWidget()==&dialog && QApplication::activeWindow()==&dialog,"Recall stole a modal dialog's focus");
    dialog.hide(); focus(main,timeline); change(edit);
    stack->setCurrentWidget(audio); stack->setCurrentWidget(edit); settle(); settle();
    require(QApplication::activeWindow()==&viewer && QApplication::focusWidget()==timecode,"Rapid page switches restored a stale target");
    // Destruction while a recall is queued must invalidate saved pointers.
    change(audio); focus(effects,search); change(edit);
    delete search; search=nullptr; change(audio);
    require(QApplication::activeWindow()==&effects,"Destroyed control invalidated its still-available host");
    memory.attach(nullptr); effects.hide(); viewer.hide(); main.hide(); settle();
    std::puts("PASS: session page focus; main/panel/viewer controls; passive reveal; fallback; newer input, deactivation, modal and rapid-switch guards");
}
