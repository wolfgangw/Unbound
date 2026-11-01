// SPDX-License-Identifier: MIT
#include "colortoolselection.h"
#include "layoutadapter.h"
#include <QButtonGroup>
#include <QPushButton>
#include <QVBoxLayout>
namespace {
class NativeToolChurn final : public QObject {
public:
    QAbstractButton *onHide=nullptr,*onShow=nullptr;
    bool eventFilter(QObject *,QEvent *event)override {
        if(event->type()==QEvent::Hide && onHide)onHide->setChecked(true);
        if(event->type()==QEvent::Show && onShow) {
            const QPointer<QAbstractButton> target=onShow;
            QTimer::singleShot(0,this,[target]{if(target)target->setChecked(true);});
        }
        return false;
    }
};
}
void testColorToolSelection() {
    using LayoutTest::require;
    QWidget main;auto mainLayout=new QVBoxLayout(&main);
    auto page=new QWidget;mainLayout->addWidget(page);auto pageLayout=new QVBoxLayout(page);
    auto bottom=new QWidget;bottom->setObjectName("frameColorBottom");pageLayout->addWidget(bottom);
    auto row=new QHBoxLayout(bottom);
    QButtonGroup left(&main),right(&main),third(&main);
    auto add=[&](const char *name,QButtonGroup &group){
        auto button=new QPushButton(name,bottom);button->setObjectName(name);button->setCheckable(true);
        row->addWidget(button);group.addButton(button);return button;
    };
    auto tracker=add("btnTracker",left),curves=add("btnCurves",left);
    auto info=add("btnInfo",right),keyframes=add("btnKeyframes",right),scopes=add("btnScopes",right);
    auto wheels=add("btnColorWheels",third),raw=add("btnScene",third);
    tracker->setChecked(true);info->setChecked(true);wheels->setChecked(true);
    NativeToolChurn churn;churn.onHide=scopes;churn.onShow=keyframes;page->installEventFilter(&churn);
    main.show();LayoutTest::settle();info->setChecked(true);
    Unbound::ColorToolSelection selection(page,&main);
    page->hide();require(scopes->isChecked(),"Fixture did not change selection during native departure");
    page->show();LayoutTest::settle();
    require(tracker->isChecked() && info->isChecked() && wheels->isChecked(),"Color return lost outgoing tool regions");
    // Later native selection churn during the bounded return is also restored.
    curves->setChecked(true);raw->setChecked(true);keyframes->setChecked(true);LayoutTest::settle();
    require(tracker->isChecked() && info->isChecked() && wheels->isChecked(),"Queued native tool changes overrode return selections");
    keyframes->click();LayoutTest::settle();require(keyframes->isChecked(),"New selector input was overridden");
    page->hide();page->show();LayoutTest::settle();require(keyframes->isChecked(),"Next return ignored the newer tool choice");
    // An interrupted return retains intent, not the transient Scopes selection.
    selection.cancel();info->setChecked(true);page->hide();page->show();page->hide();page->show();LayoutTest::settle();
    require(info->isChecked(),"Rapid page switches captured an intermediate tool");
    page->hide();page->show();selection.cancel();LayoutTest::settle();
    require(keyframes->isChecked(),"Explicit layout cancellation retained stale tool restoration");
    // A hidden/disabled native control is not force-selected.
    info->setChecked(true);page->hide();info->setEnabled(false);page->show();LayoutTest::settle();
    require(keyframes->isChecked(),"Unavailable tool was forced on");selection.cancel();info->setEnabled(true);
    info->setChecked(true);main.hide();main.show();LayoutTest::settle();
    require(keyframes->isChecked(),"Main-window hiding was treated as page departure");
    std::puts("PASS: Color tool selection, native hide/show churn, multiple regions, new input, rapid return and cancellation");
}
