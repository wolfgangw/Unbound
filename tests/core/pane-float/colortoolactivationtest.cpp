// SPDX-License-Identifier: MIT
#include "colortoolactivation.h"
#include "panecatalogue.h"
#include "layoutadapter.h"
#include <QButtonGroup>
#include <QDialog>
#include <QLineEdit>
#include <QMouseEvent>
#include <QPushButton>
#include <QVBoxLayout>

namespace {
class Picker final : public QWidget {
public:
    std::function<void()> pressed;
    void mousePressEvent(QMouseEvent *) override { if(pressed)pressed(); }
};
void press(QWidget *widget) {
    QMouseEvent event(QEvent::MouseButtonPress,QPointF(2,2),Qt::LeftButton,Qt::LeftButton,Qt::NoModifier);
    QApplication::sendEvent(widget,&event);
}
}

void testColorToolActivation() {
    using LayoutTest::require;
    using LayoutTest::settle;
    QStringList affected;
    for(const auto &tool:Unbound::PaneCatalogue::colorControls())
        if(tool.selectOnFloatingInput) {
            if(tool.objectName=="colorHDRTab")
                require(tool.wholeLayout && tool.selector=="btnHdr","Floating HDR lost its complete layout or native selector");
            else require(!tool.retainedContainer.isEmpty(),"Viewer tool activation lost its native container");
            affected.append(tool.objectName);
        }
    require(affected==QStringList({"colorHDRTab","qualTab","windowTab","trackerTab","personMaskTab"}),
        "Native tool selection escaped the validated catalogue scope");

    QWidget main;auto layout=new QVBoxLayout(&main);
    auto page=new QWidget;layout->addWidget(page);
    auto pageLayout=new QVBoxLayout(page);
    auto bottom=new QWidget;bottom->setObjectName("frameColorBottom");pageLayout->addWidget(bottom);
    auto toolbar=new QHBoxLayout(bottom);
    QButtonGroup group;
    auto add=[&](const char *name) {
        auto button=new QPushButton(name);button->setObjectName(name);button->setCheckable(true);
        toolbar->addWidget(button);group.addButton(button);return button;
    };
    auto qualifier=add("btnQualifier"),magic=add("btnPersonMask");
    QString viewerOwner;
    int selections=0;
    auto wire=[&](QAbstractButton *button,const char *name) {
        QObject::connect(button,&QAbstractButton::clicked,&main,[&,name]{viewerOwner=name;++selections;});
    };
    wire(qualifier,"Qualifier");wire(magic,"Magic Mask");
    QWidget host,other;
    host.setAttribute(Qt::WA_ShowWithoutActivating);other.setAttribute(Qt::WA_ShowWithoutActivating);
    auto hostLayout=new QVBoxLayout(&host);
    auto picker=new Picker;hostLayout->addWidget(picker);
    auto field=new QLineEdit;hostLayout->addWidget(field);
    auto dock=new Picker;dock->setObjectName("resolvePaneUndockButton");hostLayout->addWidget(dock);
    Unbound::ColorToolActivation activation(&host,page,"btnQualifier",&main);
    Unbound::ColorToolActivation otherActivation(&other,page,"btnPersonMask",&main);
    main.show();LayoutTest::activate(main);magic->click();const int initial=selections;
    host.show();other.show();settle();
    require(selections==initial,"Automatic floating shows selected a Color tool");

    LayoutTest::activate(host);
    require(viewerOwner=="Qualifier" && qualifier->isChecked(),"Host activation did not select its native tool");
    int pickerPresses=0;
    picker->pressed=[&]{
        ++pickerPresses;
        if(viewerOwner!="Qualifier")std::fprintf(stderr,"Picker press %d: owner=%s activeHost=%d selections=%d\n",
            pickerPresses,qPrintable(viewerOwner),QApplication::activeWindow()==&host,selections);
        require(viewerOwner=="Qualifier","Picker ran before its native tool was selected");
    };
    auto count=selections;press(picker);press(picker);settle();
    require(pickerPresses==2 && selections==count,"Local picker input was consumed or redundantly selected the toolbar");
    field->setFocus();QKeyEvent key(QEvent::KeyPress,Qt::Key_A,Qt::NoModifier,"a");
    QApplication::sendEvent(field,&key);
    require(field->text()=="a" && QApplication::focusWidget()==field && selections==count,
        "Color tool activation disturbed local text input/focus");

    LayoutTest::activate(other);
    require(viewerOwner=="Magic Mask","Second floating tool could not take viewer ownership");
    LayoutTest::activate(host);require(viewerOwner=="Qualifier","First floating tool could not reclaim viewer ownership");
    // Expanded Warper can own viewer gestures while Qualifier remains checked.
    LayoutTest::activate(main);viewerOwner="Warper";count=selections;
    require(qualifier->isChecked(),"Checked-but-not-owner fixture lost its native selection");
    LayoutTest::activate(host);
    require(viewerOwner=="Qualifier" && selections==count+1,"A checked selector prevented viewer-owner recovery");

    // A click can precede the deferred activation callback; select before input,
    // then discard that callback, without forwarding or eating the press.
    QApplication::setActiveWindow(&main);magic->click();QApplication::setActiveWindow(&host);
    count=selections;press(picker);settle();
    require(viewerOwner=="Qualifier" && selections==count+1,"Press/activation ordering selected twice or too late");
    QApplication::setActiveWindow(&main);magic->click();QApplication::setActiveWindow(&host);
    count=selections;int dockPresses=0;dock->pressed=[&]{++dockPresses;};press(dock);settle();
    require(dockPresses==1 && selections==count,"Unbound button input changed the native tool");

    // Child/foreign activations cannot steal selection; hiding cancels queued work.
    QEvent activate(QEvent::WindowActivate);
    QApplication::sendEvent(picker,&activate);settle();require(selections==count,"Child activation selected a tool");
    QApplication::setActiveWindow(&main);QApplication::sendEvent(&host,&activate);settle();
    require(selections==count,"Inactive host accepted a synthetic activation");
    QApplication::setActiveWindow(&host);host.hide();settle();
    require(selections==count,"Hidden host executed a pending activation");
    LayoutTest::activate(main);page->hide();host.show();QApplication::setActiveWindow(&host);settle();press(field);
    require(selections==count,"Hidden page selected a floating tool");
    LayoutTest::activate(main);page->show();settle();
    require(selections==count,"Page reveal selected a floating tool");

    qualifier->setEnabled(false);LayoutTest::activate(host);press(field);settle();
    require(selections==count,"Disabled native selector was invoked");qualifier->setEnabled(true);
    // Modal/popup priority wins even if an activation event is still queued.
    QDialog dialog(&host);dialog.setModal(true);dialog.show();settle();
    QApplication::sendEvent(&host,&activate);press(field);settle();
    require(selections==count,"Modal dialog allowed tool selection");dialog.hide();settle();
    LayoutTest::activate(host);count=selections;
    QWidget popup(&host,Qt::Popup);popup.show();settle();
    QApplication::sendEvent(&host,&activate);press(field);settle();
    require(selections==count,"Popup allowed tool selection");popup.hide();settle();

    // Hidden duplicate selectors (e.g. screen-mode variants) must not win;
    // replacing the native toolbar is discovered only at the next user choice.
    LayoutTest::activate(main);qualifier->hide();
    auto replacement=add("btnQualifier");wire(replacement,"Qualifier");replacement->show();
    magic->click();LayoutTest::activate(host);
    require(replacement->isChecked() && viewerOwner=="Qualifier","Rebuilt native selector was not discovered");
    count=selections;
    host.setProperty("unboundUndockDragging",true);LayoutTest::activate(main);magic->click();
    count=selections;LayoutTest::activate(host);
    require(selections==count,"Tearoff selected the toolbar before handover finished");
    host.setProperty("unboundUndockDragging",QVariant());press(picker);settle();
    require(viewerOwner=="Qualifier","Post-tearoff input did not select the tool");

    // Stopping during a native handler models reentrant docking. The original
    // picker still receives input and stopped/queued callbacks cannot replay.
    LayoutTest::activate(main);magic->click();
    QObject::connect(replacement,&QAbstractButton::clicked,&main,[&]{activation.stop();});
    QApplication::setActiveWindow(&host);count=selections;press(picker);settle();
    require(selections==count+1,"Reentrant docking replayed native selection");
    LayoutTest::activate(main);magic->click();count=selections;LayoutTest::activate(host);press(dock);settle();
    require(selections==count,"Stopped Color activation still selected a tool");
    otherActivation.stop();
    std::puts("PASS: floating Color native selection, input ordering, ownership, scope, lifecycle and reentrant docking");
}
