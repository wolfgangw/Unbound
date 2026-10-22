// SPDX-License-Identifier: MIT
#pragma once
#include <QFrame>
#include <QScreen>
#include <QVBoxLayout>
#include <QToolButton>
#include <QPushButton>
#include <QHBoxLayout>
#include <QLabel>
#include <QTimer>
#include <QIconEngine>
#include <QPainter>
#include <QMouseEvent>
#include <QKeyEvent>
#include <QApplication>
#include <QStyle>
#include <QMap>
#include <QLineEdit>
#include <QStackedWidget>
#include <QWindow>
#include <QWheelEvent>
#include <QContextMenuEvent>
#include <QTabletEvent>
#include <QTouchEvent>
#include <QNativeGestureEvent>
#include <QDropEvent>
#include <functional>
#include "panesettings.h"
#include "windowinteraction.h"
namespace Unbound {
class LayoutActionIcon final : public QIconEngine {
public:
    explicit LayoutActionIcon(bool save=false) : m_save(save) {}
    QIconEngine *clone() const override { return new LayoutActionIcon(m_save); }
    void paint(QPainter *p,const QRect &rect,QIcon::Mode mode,QIcon::State) override {
        p->save(); p->setRenderHint(QPainter::Antialiasing);
        const qreal scale=qMin(rect.width(),rect.height())/20.;
        p->translate(rect.center().x()-10*scale,rect.center().y()-10*scale); p->scale(scale,scale);
        p->setPen(QPen(QColor(m_save ? (mode==QIcon::Active ? "#ffffff" : "#eeeeee") :
            (mode==QIcon::Active ? "#dddddd" : "#999999")),1.4)); p->setBrush(Qt::NoBrush);
        if(m_save) {
            QPolygonF outline; outline<<QPointF(4,3.5)<<QPointF(13,3.5)<<QPointF(16,6.5)
                <<QPointF(16,16.5)<<QPointF(4,16.5)<<QPointF(4,3.5);
            p->drawPolyline(outline);
            p->drawRect(QRectF(7,3.5,5,5)); p->drawRect(QRectF(7,11,6,5.5));
            p->restore(); return;
        }
        p->drawLine(QPointF(4,5.5),QPointF(16,5.5));
        QPolygonF handle; handle<<QPointF(7,5)<<QPointF(7,3.5)<<QPointF(13,3.5)<<QPointF(13,5);
        QPolygonF body; body<<QPointF(5.5,7)<<QPointF(6,16.5)<<QPointF(14,16.5)<<QPointF(14.5,7);
        p->drawPolyline(handle); p->drawPolyline(body);
        p->drawLine(QPointF(8.5,8),QPointF(8.5,14)); p->drawLine(QPointF(11.5,8),QPointF(11.5,14));
        p->restore();
    }
    QPixmap pixmap(const QSize &size,QIcon::Mode mode,QIcon::State state) override {
        QPixmap result(size); result.fill(Qt::transparent); QPainter painter(&result);
        paint(&painter,QRect(QPoint(),size),mode,state); return result;
    }
private:
    bool m_save=false;
};
class LayoutNameButton final : public QPushButton {
public:
    explicit LayoutNameButton(QWidget *parent) : QPushButton(parent) {
        m_hold.setSingleShot(true); m_hold.setInterval(1000); m_hold.setTimerType(Qt::PreciseTimer);
        connect(&m_hold,&QTimer::timeout,this,[this] {
            if(!isDown()) return;
            m_longPress=true; setDown(false);
            if(renameRequested) renameRequested();
        });
    }
    std::function<void()> renameRequested;
protected:
    void mousePressEvent(QMouseEvent *event) override {
        m_longPress=false; m_press=event->pos();
        QPushButton::mousePressEvent(event);
        if(event->button()==Qt::LeftButton && isDown()) m_hold.start();
    }
    void mouseMoveEvent(QMouseEvent *event) override {
        if(!rect().contains(event->pos()) || (event->pos()-m_press).manhattanLength()>QApplication::startDragDistance()) m_hold.stop();
        QPushButton::mouseMoveEvent(event);
    }
    void mouseReleaseEvent(QMouseEvent *event) override {
        m_hold.stop();
        if(m_longPress) { setDown(false); event->accept(); return; }
        QPushButton::mouseReleaseEvent(event);
    }
    void hideEvent(QHideEvent *event) override { m_hold.stop(); QPushButton::hideEvent(event); }
private:
    QTimer m_hold;
    QPoint m_press;
    bool m_longPress=false;
};
class LayoutNameEdit final : public QLineEdit {
public:
    explicit LayoutNameEdit(QWidget *parent) : QLineEdit(parent) { setMaxLength(128); }
    std::function<void(bool)> finish;
    // Only the owning popout calls this, for its focused rename field. Sending
    // the event with sendEvent would re-enter Resolve's application dispatcher,
    // which can discard key presses until the field receives a real click.
    bool handlePopoutKey(QEvent *event) { return QLineEdit::event(event); }
protected:
    void keyPressEvent(QKeyEvent *event) override {
        if(hasSelectedText() && event->modifiers()==Qt::NoModifier &&
            (event->key()==Qt::Key_Left || event->key()==Qt::Key_Right)) {
            const int start=selectionStart(),end=start+selectedText().size();
            setCursorPosition(event->key()==Qt::Key_Left ? start : end);
            event->accept(); return;
        }
        if(event->key()==Qt::Key_Return || event->key()==Qt::Key_Enter || event->key()==Qt::Key_Escape) {
            if(finish) finish(event->key()!=Qt::Key_Escape);
            event->accept(); return;
        }
        QLineEdit::keyPressEvent(event);
    }
    void focusOutEvent(QFocusEvent *event) override {
        QLineEdit::focusOutEvent(event);
        if(finish) finish(false);
    }
};
// A persistent Qt tool popout, deliberately outside Qt's transient-popup stack.
// Resolve may close that stack while docking panels; our outside-input filter
// supplies dismissal without an intervening hide/reopen during restoration.
class PageLayoutMenu final : public QFrame {
public:
    std::function<QString(QString &,bool &)> save;
    std::function<QString(const QString &,bool &)> recall;
    std::function<QString(const QString &)> remove;
    std::function<QString(const QString &)> update;
    std::function<QString()> dockAll;
    std::function<QString(const QString &,const QString &)> rename;
    std::function<QString()> dockingWarning;
    PageLayoutMenu(PaneSettings &settings,QString page,QWidget *parent,std::function<bool()> hasFloating={})
        : QFrame(parent),m_settings(settings),m_page(std::move(page)),m_hasFloating(std::move(hasFloating)) {
        setWindowFlags(Qt::Tool | Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint);
        setObjectName("unboundLayoutMenu");
        setWindowTitle(PaneSettings::pageLabel(m_page)+" panel configurations");
        setAttribute(Qt::WA_TranslucentBackground);
        setAttribute(Qt::WA_QuitOnClose,false);
        setFocusPolicy(Qt::StrongFocus);
        m_layout=new QVBoxLayout(this); m_layout->setContentsMargins(6,6,6,6); m_layout->setSpacing(0);
        m_dockingWarning=new QLabel(this); m_dockingWarning->setObjectName("unboundDockingWarning");
        m_dockingWarning->setTextFormat(Qt::PlainText); m_dockingWarning->setWordWrap(true);
        m_dockingWarning->setMaximumWidth(300); m_dockingWarning->hide(); addWidget(m_dockingWarning);
        // Resolve supplies the theme; do not maintain a separate popup grey.
        m_background=(parent ? parent->palette() : qApp->palette()).color(QPalette::Window);
        setStyleSheet("QFrame#unboundLayoutMenu { background: transparent; border: none; }"
            "QFrame#unboundLayoutMenu QAbstractButton { color: #dddddd; background: transparent; border: none; border-radius: 3px; padding: 6px 8px; text-align: left; }"
            "QFrame#unboundLayoutMenu QAbstractButton:hover { background: #414141; }"
            "QFrame#unboundLayoutMenu QAbstractButton:focus { background: #414141; }"
            "QFrame#unboundLayoutMenu QAbstractButton[activeLayout='true'] { background: #40566a; color: #ffffff; }"
            "QFrame#unboundLayoutMenu QAbstractButton:disabled { color: #777777; }"
            "QFrame#unboundLayoutMenu QToolButton#unboundLayoutUpdate { background: #ed4b3f; }"
            "QFrame#unboundLayoutMenu QToolButton#unboundLayoutUpdate:hover { background: #f65b4f; }"
            "QFrame#unboundLayoutMenu QToolButton#unboundLayoutUpdate:focus { background: #f65b4f; }"
            "QFrame#unboundLayoutMenu QLineEdit { color: #dddddd; background: #292929; border: 1px solid #777777; border-radius: 3px; padding: 4px 7px; selection-background-color: #40566a; }"
            "QFrame#unboundLayoutMenu QLabel { color: #999999; padding: 5px 8px; background: transparent; }");
        m_save=new QPushButton(this); m_save->setObjectName("unboundLayoutSave");
        m_save->setText(QStringLiteral("Save as new %1 layout").arg(PaneSettings::pageLabel(m_page)));
        m_save->setAutoDefault(false); m_save->setMinimumWidth(275);
        addWidget(m_save);
        auto dock=new QPushButton("Dock all",this); dock->setObjectName("unboundLayoutDockAll");
        dock->setAutoDefault(false); m_dock=dock; addWidget(dock);
        connect(dock,&QPushButton::clicked,this,[this] {
            m_recalling=true;
            const auto error=dockAll ? dockAll() : QStringLiteral("Dock all is unavailable.");
            if(!error.isEmpty()) finishRecall();
            afterAction(error);
        });
        auto separator=new QFrame(this); separator->setFixedHeight(1);
        separator->setStyleSheet("background: #444444; border: none;");
        m_layout->addSpacing(4); addWidget(separator); m_layout->addSpacing(4);
        m_status=new QLabel(this); m_status->setObjectName("unboundLayoutStatus");
        m_status->setTextFormat(Qt::PlainText); m_status->setWordWrap(true); m_status->setMaximumWidth(300);
        addWidget(m_status);
        connect(m_save,&QPushButton::clicked,this,[this] {
            if(!save) return;
            QString name; bool duplicate=false;
            const auto error=save(name,duplicate);
            afterAction(error.isEmpty() ? (duplicate ? "Already saved as " : "Saved as ")+name : error);
        });
        refresh();
    }
    ~PageLayoutMenu() override {
        // Focus-out can occur while QWidget destroys its children, after our
        // members have gone. Disconnect inline-edit callbacks before that.
        for(const auto &row:m_rows) { row.editor->finish={}; row.load->renameRequested={}; }
        m_editing.clear();
        if(qApp) qApp->removeEventFilter(this);
    }
    QString page() const { return m_page; }
    void refreshDockingWarning() {
        const auto message=dockingWarning ? dockingWarning() : QString();
        m_dockingWarning->setText(message); m_dockingWarning->setVisible(!message.isEmpty());
        refresh(); // Recovery can also change Dock all availability and layout dirtiness.
        adjustSize();
    }
    void popup(const QPoint &position,QWidget *invokingButton=nullptr) {
        refreshDockingWarning();
        // Cocoa delivers the context request on press, Windows on release.
        // The opening gesture is not a new outside interaction. Keep its release
        // exempt through both QWindow and QWidget delivery; the next press ends
        // the exemption. No timer or native platform special case is needed.
        m_openingButtons=invokingButton ? QApplication::mouseButtons() : Qt::NoButton;
        m_openingArea=invokingButton ? QRect(invokingButton->mapToGlobal(QPoint()),invokingButton->size()) : QRect();
        adjustSize();
        auto screen=QGuiApplication::screenAt(position);
        if(!screen) screen=QGuiApplication::primaryScreen();
        QPoint target=position;
        if(screen) {
            const auto area=screen->availableGeometry();
            target.setX(qBound(area.left(),target.x(),qMax(area.left(),area.right()-width()+1)));
            target.setY(qBound(area.top(),target.y(),qMax(area.top(),area.bottom()-height()+1)));
        }
        move(target); show(); raise();
        setFocus(Qt::PopupFocusReason);
    }
    void finishRecall() {
        if(!m_recalling) return;
        m_recalling=false;
        afterAction({});
    }
protected:
    void paintEvent(QPaintEvent *) override {
        // Paint the translucent tool surface explicitly; QFrame's default
        // NoFrame style need not paint a stylesheet background on Windows.
        QPainter painter(this); painter.setRenderHint(QPainter::Antialiasing);
        painter.setPen(QPen(QColor("#555555"),1)); painter.setBrush(m_background);
        painter.drawRoundedRect(QRectF(rect()).adjusted(.5,.5,-.5,-.5),4,4);
    }
    void showEvent(QShowEvent *event) override {
        qApp->installEventFilter(this); QFrame::showEvent(event);
    }
    void hideEvent(QHideEvent *event) override {
        m_openingButtons=Qt::NoButton;
        m_recalling=false;
        finishRename(false);
        if(qApp) qApp->removeEventFilter(this);
        QFrame::hideEvent(event);
    }
    bool eventFilter(QObject *object,QEvent *event) override {
        if(isVisible() && event->type()==WindowInteractionEvent::eventType() && object!=this)
            hide();
        // Both native and Qt focus already belong to the editor. Resolve can
        // nevertheless intercept key presses before widget filters, including
        // keys forwarded via sendEvent. Deliver this popout's rename input to
        // our own editor handler without re-entering application dispatch.
        if(isVisible() && isActiveWindow() && !m_editing.isEmpty() && object==windowHandle() &&
            (event->type()==QEvent::KeyPress || event->type()==QEvent::KeyRelease ||
             event->type()==QEvent::ShortcutOverride)) {
            auto editor=m_rows.value(m_editing).editor;
            if(editor && editor->isVisible() && editor->hasFocus()) {
                editor->handlePopoutKey(event);
                return true;
            }
        }
        if(isVisible() && (outsideInteraction(object,event) ||
            event->type()==QEvent::ApplicationDeactivate ||
            (event->type()==QEvent::KeyPress && static_cast<QKeyEvent *>(event)->key()==Qt::Key_Escape && m_editing.isEmpty()))) hide();
        return QFrame::eventFilter(object,event);
    }
private:
    bool ownsInput(QObject *object) const {
        if(object==windowHandle()) return true;
        auto widget=qobject_cast<QWidget *>(object);
        return widget && (widget==this || isAncestorOf(widget));
    }
    bool outsideInteraction(QObject *object,QEvent *event) {
        auto outside=[this](QPoint point) { return !geometry().contains(point); };
        switch(event->type()) {
        case QEvent::MouseButtonPress: case QEvent::MouseButtonDblClick:
        case QEvent::NonClientAreaMouseButtonPress:
        case QEvent::NonClientAreaMouseButtonDblClick:
            m_openingButtons=Qt::NoButton;
            return outside(static_cast<QMouseEvent *>(event)->globalPos());
        case QEvent::MouseButtonRelease: case QEvent::NonClientAreaMouseButtonRelease: {
            auto mouse=static_cast<QMouseEvent *>(event);
            if(m_openingButtons & mouse->button()) return false;
            return outside(mouse->globalPos());
        }
        case QEvent::MouseMove: case QEvent::NonClientAreaMouseMove: {
            auto mouse=static_cast<QMouseEvent *>(event);
            if((mouse->buttons() & m_openingButtons) && !(mouse->buttons() & ~m_openingButtons)
                && m_openingArea.contains(mouse->globalPos())) return false;
            return mouse->buttons()!=Qt::NoButton && outside(mouse->globalPos());
        }
        case QEvent::Wheel: return outside(static_cast<QWheelEvent *>(event)->globalPosition().toPoint());
        case QEvent::ContextMenu: {
            auto context=static_cast<QContextMenuEvent *>(event);
            return context->reason()==QContextMenuEvent::Keyboard ? !ownsInput(object) : outside(context->globalPos());
        }
        case QEvent::TabletPress: case QEvent::TabletRelease:
            return outside(static_cast<QTabletEvent *>(event)->globalPosF().toPoint());
        case QEvent::TabletMove: {
            auto tablet=static_cast<QTabletEvent *>(event);
            return tablet->buttons()!=Qt::NoButton && outside(tablet->globalPosF().toPoint());
        }
        case QEvent::TouchBegin: case QEvent::TouchUpdate: case QEvent::TouchEnd:
            for(const auto &point:static_cast<QTouchEvent *>(event)->touchPoints())
                if(outside(point.screenPos().toPoint())) return true;
            return false;
        case QEvent::NativeGesture:
            return outside(static_cast<QNativeGestureEvent *>(event)->globalPos());
        case QEvent::DragEnter: case QEvent::DragMove: case QEvent::Drop: {
            const auto point=static_cast<QDropEvent *>(event)->pos();
            if(auto widget=qobject_cast<QWidget *>(object)) return outside(widget->mapToGlobal(point));
            if(auto window=qobject_cast<QWindow *>(object)) return outside(window->mapToGlobal(point));
            return !ownsInput(object);
        }
        case QEvent::KeyPress: case QEvent::KeyRelease: case QEvent::ShortcutOverride:
        case QEvent::Shortcut: case QEvent::InputMethod: case QEvent::Gesture:
        case QEvent::ScrollPrepare: case QEvent::Scroll:
            return !ownsInput(object);
        default: return false; // Hover and programmatic layout/activation are not input.
        }
    }
    void addWidget(QWidget *widget) { m_layout->addWidget(widget); }
    void afterAction(QString message) {
        // Recall and its completion can request refresh in the same event turn.
        // Commit one update, preserving the existing rows.
        if(!message.isEmpty()) m_message=message;
        if(m_refreshQueued) return;
        m_refreshQueued=true;
        QTimer::singleShot(0,this,[this] {
            m_refreshQueued=false;
            refresh();
            if(!m_message.isEmpty()) m_status->setText(m_message);
            m_message.clear(); adjustSize();
        });
    }
    struct LayoutRow {
        QWidget *widget=nullptr;
        LayoutNameButton *load=nullptr;
        QToolButton *update=nullptr,*trash=nullptr;
        QStackedWidget *nameArea=nullptr;
        LayoutNameEdit *editor=nullptr;
    };
    void beginRename(const QString &id) {
        if(!m_rows.contains(id)) return;
        finishRename(false);
        const auto &row=m_rows[id];
        m_editing=id;
        row.editor->setText(m_settings.layoutName(m_page,id));
        row.nameArea->setCurrentWidget(row.editor);
        // A persistent tool popout can remain inactive while Resolve owns
        // keyboard focus. setFocus alone then only selects a future focus
        // widget; explicitly activate the editor's top-level window as well.
        activateWindow();
        row.editor->setFocus(Qt::MouseFocusReason); row.editor->selectAll();
        // Filters run newest first. Keep the narrowly scoped editor handoff
        // ahead of any filters Resolve installed since this popout opened.
        qApp->installEventFilter(this);
    }
    void finishRename(bool submit) {
        if(m_editing.isEmpty() || !m_rows.contains(m_editing)) return;
        const auto id=m_editing;
        const auto &row=m_rows[id];
        if(submit) {
            const auto error=rename ? rename(id,row.editor->text()) : m_settings.renameLayout(m_page,id,row.editor->text());
            if(!error.isEmpty()) { m_status->setText(error); row.editor->setFocus(); return; }
        }
        m_editing.clear();
        const bool focused=row.editor->hasFocus();
        row.nameArea->setCurrentWidget(row.load);
        if(focused) setFocus(Qt::OtherFocusReason);
        if(submit || isVisible()) afterAction({});
    }
    LayoutRow makeRow(const QString &id) {
        auto row=new QWidget(this); auto layout=new QHBoxLayout(row);
        layout->setContentsMargins(0,2,0,2); layout->setSpacing(5);
        auto nameArea=new QStackedWidget(row); layout->addWidget(nameArea,1);
        auto load=new LayoutNameButton(nameArea); load->setObjectName("unboundLayoutRecall"); load->setAutoDefault(false);
        load->setSizePolicy(QSizePolicy::Ignored,QSizePolicy::Preferred); nameArea->addWidget(load);
        load->setProperty("layoutId",id);
        auto editor=new LayoutNameEdit(nameArea); editor->setObjectName("unboundLayoutNameEdit");
        editor->setProperty("layoutId",id); nameArea->addWidget(editor);
        load->renameRequested=[this,id] { beginRename(id); };
        editor->finish=[this](bool submit) { finishRename(submit); };
        auto updateButton=new QToolButton(row); updateButton->setObjectName("unboundLayoutUpdate");
        updateButton->setProperty("layoutId",id);
        updateButton->setIcon(QIcon(new LayoutActionIcon(true))); updateButton->setIconSize(QSize(18,18));
        updateButton->setFixedSize(30,30); layout->addWidget(updateButton);
        auto trash=new QToolButton(row); trash->setObjectName("unboundLayoutDelete");
        trash->setProperty("layoutId",id);
        trash->setIcon(QIcon(new LayoutActionIcon)); trash->setIconSize(QSize(18,18)); trash->setFixedSize(30,30);
        layout->addWidget(trash);
        m_layout->insertWidget(m_layout->indexOf(m_status),row);
        connect(load,&QPushButton::clicked,this,[this,id] {
            m_recalling=true;
            bool applied=false;
            const auto error=recall ? recall(id,applied) : QStringLiteral("Recall is unavailable.");
            if(error.isEmpty() && !applied) { m_recalling=false; return; }
            if(!error.isEmpty()) finishRecall();
            afterAction(error);
        });
        connect(updateButton,&QToolButton::clicked,this,[this,id] {
            const auto error=update ? update(id) : m_settings.updateLayout(m_page,id);
            afterAction(error);
        });
        connect(trash,&QToolButton::clicked,this,[this,id] {
            const auto name=m_settings.layoutName(m_page,id);
            const auto error=remove ? remove(id) : m_settings.deleteLayout(m_page,id);
            afterAction(error.isEmpty() ? "Deleted "+name : error);
        });
        return {row,load,updateButton,trash,nameArea,editor};
    }
    void refresh() {
        // Keep rows stable and update their styling in a single paint pass.
        const bool painting=updatesEnabled();
        if(painting) setUpdatesEnabled(false);
        const bool floating=m_hasFloating && m_hasFloating();
        m_dock->setEnabled(floating); m_dock->setVisible(floating);
        const auto entries=m_settings.layouts(m_page);
        QSet<QString> ids;
        for(const auto &value:entries) ids.insert(value.toObject().value("id").toString());
        for(auto it=m_rows.begin();it!=m_rows.end();) {
            if(ids.contains(it.key())) { ++it; continue; }
            if(m_editing==it.key()) m_editing.clear();
            auto widget=it.value().widget; m_layout->removeWidget(widget); widget->hide();
            widget->deleteLater(); it=m_rows.erase(it);
        }
        for(const auto &value:entries) {
            const auto item=value.toObject(); const auto id=item.value("id").toString();
            const auto name=item.value("name").toString();
            if(!m_rows.contains(id)) m_rows.insert(id,makeRow(id));
            const auto &row=m_rows[id];
            auto label=name; label.replace("&","&&"); row.load->setText(label);
            row.load->setAccessibleName("Recall "+name); row.load->setToolTip(name.toHtmlEscaped()+"<br/>Hold 1 second to rename");
            row.editor->setAccessibleName("Layout name: "+name);
            row.update->setToolTip("<qt>"+("Update "+name).toHtmlEscaped()+"</qt>"); row.update->setAccessibleName("Update "+name);
            row.trash->setToolTip("<qt>"+("Delete "+name).toHtmlEscaped()+"</qt>"); row.trash->setAccessibleName("Delete "+name);
            const bool active=m_settings.activeLayout(m_page)==id;
            if(!row.load->property("activeLayout").isValid() || row.load->property("activeLayout").toBool()!=active) {
                row.load->setProperty("activeLayout",active);
                row.load->style()->unpolish(row.load); row.load->style()->polish(row.load); row.load->update();
            }
            row.update->setVisible(active && m_settings.layoutDirty(m_page,id));
        }
        m_save->setEnabled(entries.size()<5);
        m_save->setToolTip(entries.size()<5 ? QStringLiteral("Create a new layout. Use the save icon beside the active layout to update it.") : QStringLiteral("Delete a layout to make room."));
        m_status->setText(entries.isEmpty() ? "No saved layouts" : QStringLiteral("%1 / 5 layouts").arg(entries.size()));
        if(painting) setUpdatesEnabled(true);
    }
    QColor m_background;
    PaneSettings &m_settings;
    QString m_page;
    QPushButton *m_save=nullptr;
    QLabel *m_status=nullptr;
    QLabel *m_dockingWarning=nullptr;
    QPushButton *m_dock=nullptr;
    QVBoxLayout *m_layout=nullptr;
    std::function<bool()> m_hasFloating;
    QMap<QString,LayoutRow> m_rows;
    QString m_message,m_editing;
    Qt::MouseButtons m_openingButtons=Qt::NoButton;
    QRect m_openingArea;
    bool m_recalling=false,m_refreshQueued=false;
};
}
