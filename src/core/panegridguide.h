// SPDX-License-Identifier: MIT
#pragma once
#include "panesnap.h"
#include "gridreferenceevent.h"
#include <QWidget>
#include <QPainter>
#include "gridreferenceimage.h"
#include <QPropertyAnimation>
#include <memory>
namespace Unbound {
class PaneGridGuide final : public QWidget {
    QVector<QRect> references;
    QImage referenceImage;
    QRect cachedArea;
    qreal cachedScale=0;
    QPropertyAnimation fade{this,"windowOpacity"};
    bool wanted=false;
    void animate(qreal opacity) {
        fade.stop();fade.setDuration(100);
        fade.setStartValue(windowOpacity());fade.setEndValue(opacity);fade.start();
    }
    void captureReferences() {
        references=GridReferenceEvent::request();referenceImage={};
    }
public:
    PaneGridGuide():QWidget(nullptr,Qt::Tool|Qt::FramelessWindowHint|Qt::WindowTransparentForInput|Qt::WindowDoesNotAcceptFocus) {
        setAttribute(Qt::WA_TranslucentBackground);setAttribute(Qt::WA_ShowWithoutActivating);
        setAttribute(Qt::WA_TransparentForMouseEvents);setAttribute(Qt::WA_QuitOnClose,false);
        setFocusPolicy(Qt::NoFocus);setObjectName("unboundGridGuide");
        connect(&fade,&QPropertyAnimation::finished,this,[this] {if(!wanted) {QWidget::hide();referenceImage={};}});
    }
    void appear() {
        if(wanted)return;
        wanted=true;captureReferences();update();
        if(!isVisible()) {setWindowOpacity(0);QWidget::show();raise();}
        animate(1);
    }
    void disappear() {
        if(!wanted)return;
        wanted=false;animate(0);
    }
protected:
    void paintEvent(QPaintEvent *) override {
        QPainter p(this);p.setPen(QColor(180,180,180,45));
        for(int x=0;x<width();x+=PaneGridSpacing)p.drawLine(x,0,x,height());
        for(int y=0;y<height();y+=PaneGridSpacing)p.drawLine(0,y,width(),y);
        if(referenceImage.isNull() || cachedArea!=geometry() || cachedScale!=devicePixelRatioF()) {
            cachedArea=geometry();cachedScale=devicePixelRatioF();
            referenceImage=gridReferenceImage(references,cachedArea,cachedScale);
        }
        p.drawImage(QPoint(0,0),referenceImage);
    }
};
class PaneGridPresentation {
    std::unique_ptr<PaneGridGuide> guide;
public:
    void hide() {if(guide)guide->disappear();}
    void show(QRect area) {
        if(area.isEmpty()) {hide();return;}
        if(!guide)guide=std::make_unique<PaneGridGuide>();
        if(guide->geometry()!=area)guide->setGeometry(area);
        guide->appear();
    }
};
}
