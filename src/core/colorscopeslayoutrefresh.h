// SPDX-License-Identifier: MIT
#pragma once
#include <QApplication>
#include <QMouseEvent>
#include <QPointer>
#include <QTimer>
#include <QWidget>
#include "widgetquirks.h"

namespace Unbound {
// Native Scopes refreshes its view-count buttons in its custom border gesture,
// not resizeEvent. Keep that native policy without adopting its aspect ratio.
class ColorScopesLayoutRefresh final : public QObject {
public:
    ColorScopesLayoutRefresh(QWidget *dialog,QWidget *host,QObject *owner)
        : QObject(owner),m_dialog(dialog),m_host(host),m_border(dialog->findChild<QWidget *>("frameBorder")) {
        rememberCursor(m_dialog,m_dialogCursor);
        rememberCursor(m_border,m_borderCursor);
        qApp->installEventFilter(this);
        request();
        m_quirks=attachScopesWidgetQuirks(dialog,host);
    }
    ~ColorScopesLayoutRefresh() override { if(qApp)qApp->removeEventFilter(this); }
protected:
    bool eventFilter(QObject *object,QEvent *event) override {
        if(m_refreshing)return false;
        // The embedded dialog is content, not a second movable/resizable
        // window. Child controls/plots keep their input; only events delivered
        // or propagated to the native window itself must stop here. Our direct
        // refresh dispatch below deliberately bypasses these application filters.
        if(object==m_dialog && m_dialog && m_dialog->parentWidget()==m_host
            && (event->type()==QEvent::MouseButtonPress || event->type()==QEvent::MouseButtonRelease
                || event->type()==QEvent::MouseButtonDblClick || event->type()==QEvent::MouseMove)) {
            event->accept();return true;
        }
        if(event->type()==QEvent::CursorChange) {
            if(object==m_dialog)rememberCursor(m_dialog,m_dialogCursor);
            else if(object==m_border)rememberCursor(m_border,m_borderCursor);
        }
        if(object==m_dialog) {
            if(event->type()==QEvent::Resize || event->type()==QEvent::Show)request();
            else if(event->type()==QEvent::Hide || event->type()==QEvent::ParentChange) {
                m_pending=false;m_lastSize={};
            }
        }
        if(m_pending && event->type()==QEvent::MouseButtonRelease)queue();
        return false;
    }
private:
    struct CursorState { QCursor cursor;bool explicitSet=false; };
    static bool resizeCursor(Qt::CursorShape shape) {
        return shape==Qt::SizeHorCursor || shape==Qt::SizeVerCursor
            || shape==Qt::SizeBDiagCursor || shape==Qt::SizeFDiagCursor;
    }
    static void rememberCursor(QWidget *widget,CursorState &state) {
        if(!widget || resizeCursor(widget->cursor().shape()))return;
        state={widget->cursor(),widget->testAttribute(Qt::WA_SetCursor)};
    }
    static void restoreCursor(QWidget *widget,const CursorState &state) {
        if(!widget)return;
        if(state.explicitSet)widget->setCursor(state.cursor);else widget->unsetCursor();
    }
    void request() { m_pending=true;queue(); }
    void queue() {
        if(m_queued)return;
        m_queued=true;
        QTimer::singleShot(0,this,[this]{m_queued=false;refresh();});
    }
    void refresh() {
        if(!m_pending || !m_dialog || !m_host || !m_border
            || m_dialog->parentWidget()!=m_host || !m_dialog->isVisible())return;
        // Never compete with a real native gesture or a control's mouse grab.
        // Its release schedules the outstanding refresh; there is no polling.
        if(QApplication::mouseButtons()!=Qt::NoButton || QWidget::mouseGrabber())return;
        m_pending=false;
        if(m_dialog->size()==m_lastSize)return;
        const auto geometry=m_dialog->geometry();
        // A real corner hover can leave the dialog carrying its temporary
        // resize cursor when this deferred refresh starts. Do not make that
        // transient cursor the permanent inherited cursor for all scope plots.
        rememberCursor(m_dialog,m_dialogCursor);
        rememberCursor(m_border,m_borderCursor);
        const QPoint local=m_border->mapTo(m_dialog,QPoint(m_border->width()-2,m_border->height()-2));
        const QPoint global=m_dialog->mapToGlobal(local);
        QMouseEvent press(QEvent::MouseButtonPress,local,global,Qt::LeftButton,Qt::LeftButton,Qt::NoModifier);
        QMouseEvent move(QEvent::MouseMove,local,global,Qt::NoButton,Qt::LeftButton,Qt::NoModifier);
        QMouseEvent release(QEvent::MouseButtonRelease,local,global,Qt::LeftButton,Qt::NoButton,Qt::NoModifier);
        m_refreshing=true;
        // Public QObject virtual dispatch, scoped to this native widget. Going
        // through QApplication would expose synthetic input to menu dismissal,
        // focus tracking and geometry-restore cancellation filters.
        static_cast<QObject *>(m_dialog.data())->event(&press);
        if(m_dialog)static_cast<QObject *>(m_dialog.data())->event(&move);
        if(m_dialog)static_cast<QObject *>(m_dialog.data())->event(&release);
        if(m_dialog && m_dialog->parentWidget()==m_host) {
            m_dialog->setGeometry(geometry);
            restoreCursor(m_dialog,m_dialogCursor);
            m_lastSize=m_dialog->size();
        }
        restoreCursor(m_border,m_borderCursor);
        m_refreshing=false;
    }
    std::unique_ptr<QObject> m_quirks;
    QPointer<QWidget> m_dialog,m_host,m_border;
    QSize m_lastSize;
    CursorState m_dialogCursor,m_borderCursor;
    bool m_pending=false,m_queued=false,m_refreshing=false;
};
}
