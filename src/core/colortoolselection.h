// SPDX-License-Identifier: MIT
#pragma once
#include <QApplication>
#include <QAbstractButton>
#include <QPointer>
#include <QScopedValueRollback>
#include <QTimer>
#include "panecatalogue.h"
#include "layoutreadiness.h"
namespace Unbound {
// Session-only native tool choices. Scopes handoff can select a different tool
// during page hide/show; preserve all visible native regions, without assuming
// a two- or three-region layout or adding these choices to saved layouts.
class ColorToolSelection final : public QObject {
public:
    ColorToolSelection(QWidget *page,QObject *owner):QObject(owner),m_page(page) {
        m_deadline.setSingleShot(true);m_deadline.setInterval(1000);
        connect(&m_deadline,&QTimer::timeout,this,[this]{cancel();});
        qApp->installEventFilter(this);
    }
    ~ColorToolSelection() override {cancel();if(qApp)qApp->removeEventFilter(this);}
    void cancel() {stop();m_selected.clear();m_buttons.clear();}
protected:
    bool eventFilter(QObject *object,QEvent *event) override {
        if(!m_page || QCoreApplication::closingDown())return false;
        if(object==m_page) {
            if(event->type()==QEvent::Hide) {
                auto main=m_page->window();
                if(!main->isVisible() || main->isMinimized()){cancel();return false;}
                // A rapid departure must not capture an intermediate selection
                // from the return that has just been superseded.
                if(!m_returning)capture();
                stop();
            } else if(event->type()==QEvent::Show && !m_selected.isEmpty()) {
                stop();m_returning=true;m_deadline.start();
                for(auto button:m_buttons)if(button) {
                    m_connections.append(connect(button,&QAbstractButton::toggled,this,[this]{
                        if(!m_applying)queue();
                    }));
                    m_connections.append(connect(button,&QAbstractButton::clicked,this,[this]{
                        if(!m_applying)cancel();
                    }));
                }
                queue();
            }
        }
        // Observe input only to yield to a newer choice; never consume it.
        if(m_returning && (event->type()==QEvent::MouseButtonPress
            || event->type()==QEvent::KeyPress || event->type()==QEvent::ApplicationDeactivate))cancel();
        return false;
    }
private:
    void capture() {
        m_selected.clear();m_buttons.clear();
        auto bottom=m_page->findChild<QWidget *>("frameColorBottom");
        if(!bottom)return;
        for(auto button:bottom->findChildren<QAbstractButton *>()) {
            bool known=false;
            for(const auto &tool:PaneCatalogue::colorControls())
                if(button->objectName()==tool.selector){known=true;break;}
            if(!known || !button->isCheckable() || !button->isVisibleTo(m_page))continue;
            m_buttons.append(button);
            if(button->isChecked())m_selected.append(button);
        }
    }
    void stop() {
        ++m_generation;m_returning=false;m_queued=false;m_deadline.stop();m_work.clear();
        for(const auto &connection:m_connections)disconnect(connection);
        m_connections.clear();
    }
    void queue() {
        if(!m_returning || m_queued)return;
        m_queued=true;m_work.set(m_page);
        const auto generation=m_generation;
        // Let the native Show and queued floating-content reacquisition finish.
        QTimer::singleShot(0,this,[this,generation]{
            if(generation!=m_generation)return;
            QTimer::singleShot(0,this,[this,generation]{
                if(generation!=m_generation)return;
                m_queued=false;
                if(m_returning && m_page && m_page->isVisible()) {
                    QScopedValueRollback<bool> applying(m_applying,true);
                    const auto selected=m_selected;
                    for(auto button:selected) {
                        if(generation!=m_generation)break;
                        if(button && button->isVisibleTo(m_page) && button->isEnabled()
                            && !button->isChecked())button->click();
                    }
                }
                if(generation==m_generation)m_work.clear();
            });
        });
    }
    QPointer<QWidget> m_page;
    QList<QPointer<QAbstractButton>> m_selected,m_buttons;
    QList<QMetaObject::Connection> m_connections;
    QTimer m_deadline;
    LayoutPendingWork m_work;
    unsigned m_generation=0;
    bool m_returning=false,m_queued=false,m_applying=false;
};
}
