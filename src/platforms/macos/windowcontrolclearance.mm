// SPDX-License-Identifier: MIT
#import <AppKit/AppKit.h>
#include "windowcontrolclearance.h"
#include <QLayout>
#include <QPointer>
#include <QTimer>
#include <QEvent>
#include <QVector>
#include <cmath>
namespace Unbound {
class CocoaWindowControlClearance final : public QObject {
    QPointer<QWidget> m_row;
    QVector<QPointer<QWidget>> m_observed;
    QMargins m_original;
    bool m_pending=false;
    void observeAncestors() {
        for(const auto &widget:m_observed)if(widget)widget->removeEventFilter(this);
        m_observed.clear();
        for(auto widget=m_row.data();widget;widget=widget->parentWidget()) {
            m_observed.append(widget);widget->installEventFilter(this);
        }
    }
    void update() {
        if(!m_row || !m_row->layout())return;
        auto margins=m_original;
        auto top=m_row->window();
        if(top->testAttribute(Qt::WA_WState_Created)) {
            NSWindow *window=reinterpret_cast<NSView *>(top->effectiveWinId()).window;
            const auto origin=m_row->mapToGlobal(QPoint());
            const QRect rowRect(origin,m_row->size());
            const CGFloat desktopTop=NSMaxY(NSScreen.screens.firstObject.frame);
            for(auto type:{NSWindowCloseButton,NSWindowMiniaturizeButton,NSWindowZoomButton}) {
                NSButton *button=[window standardWindowButton:type];
                if(!button || button.hiddenOrHasHiddenAncestor)continue;
                const NSRect frame=[window convertRectToScreen:[button convertRect:button.bounds toView:nil]];
                const QRect controls(QPoint(int(std::floor(NSMinX(frame))),int(std::floor(desktopTop-NSMaxY(frame)))),
                    QPoint(int(std::ceil(NSMaxX(frame)))-1,int(std::ceil(desktopTop-NSMinY(frame)))-1));
                if(rowRect.intersects(controls))
                    margins.setLeft(qMax(margins.left(),controls.right()+1-origin.x()+m_original.left()));
            }
        }
        if(m_row->layout()->contentsMargins()!=margins)m_row->layout()->setContentsMargins(margins);
    }
    void schedule() {
        if(m_pending)return;
        m_pending=true;
        QTimer::singleShot(0,this,[this] {m_pending=false;update();});
    }
    bool eventFilter(QObject *,QEvent *event) override {
        switch(event->type()) {
        case QEvent::ParentChange: observeAncestors();schedule();break;
        case QEvent::Move: case QEvent::Resize: case QEvent::Show:
        case QEvent::WindowStateChange: case QEvent::WinIdChange:
        case QEvent::LayoutRequest: schedule();break;
        default:break;
        }
        return false;
    }
public:
    explicit CocoaWindowControlClearance(QWidget *row):m_row(row),m_original(row->layout()->contentsMargins()) {
        observeAncestors();
        update();schedule();
    }
    ~CocoaWindowControlClearance() override {
        for(const auto &widget:m_observed)if(widget)widget->removeEventFilter(this);
        if(m_row && m_row->layout())m_row->layout()->setContentsMargins(m_original);
    }
};
std::unique_ptr<QObject> reserveWindowControlClearance(QWidget *row) {
    if(!row || !row->layout())return {};
    return std::make_unique<CocoaWindowControlClearance>(row);
}
}
