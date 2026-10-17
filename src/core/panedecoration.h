// SPDX-License-Identifier: MIT
#ifndef UNBOUND_PANEDECORATION_H
#define UNBOUND_PANEDECORATION_H
#include <QWidget>
#include <QToolButton>
#include <QAbstractButton>
#include <QBoxLayout>
#include <QPointer>
#include <QEvent>
#include <QIconEngine>
#include <QPainter>
#include <QPainterPath>
#include <QMouseEvent>
#include <QContextMenuEvent>
#include "windowintegration.h"
#include "paneheaderdrag.h"
#include <functional>
namespace Unbound {
// Proposal 5, rendered as vectors without adding a QtSvg runtime dependency.
class UndockIconEngine : public QIconEngine
{
public:
    QIconEngine *clone() const override { return new UndockIconEngine; }
    void paint(QPainter *p,const QRect &r,QIcon::Mode mode,QIcon::State) override
    {
        p->save(); p->setRenderHint(QPainter::Antialiasing);
        const qreal scale=qMin(r.width(),r.height())/20.0;
        p->translate(r.x()+(r.width()-20*scale)/2,r.y()+(r.height()-20*scale)/2); p->scale(scale,scale);
        const int brightness=mode==QIcon::Disabled ? 90 : mode==QIcon::Active ? 220 : 132;
        p->setPen(QPen(QColor(brightness,brightness,brightness),1.5,Qt::SolidLine,Qt::FlatCap,Qt::MiterJoin));
        p->setBrush(Qt::NoBrush);
        QPainterPath large; large.moveTo(15.5,7); large.lineTo(15.5,4.75); large.quadTo(15.5,3.5,14.25,3.5);
        large.lineTo(3.75,3.5); large.quadTo(2.5,3.5,2.5,4.75); large.lineTo(2.5,15.25);
        large.quadTo(2.5,16.5,3.75,16.5); large.lineTo(14.25,16.5); large.quadTo(15.5,16.5,15.5,15.25); large.lineTo(15.5,13);
        p->drawPath(large); p->drawRoundedRect(QRectF(12.5,7,6,6),.75,.75); p->restore();
    }
    QPixmap pixmap(const QSize &size,QIcon::Mode mode,QIcon::State state) override
    { QPixmap pm(size); pm.fill(Qt::transparent); QPainter p(&pm); paint(&p,QRect(QPoint(),size),mode,state); return pm; }
};
// Paint only the glyph: Resolve's inherited button styles must not add a
// hover/checked background. QToolButton still supplies clicks and tooltips.
class UndockButton : public QToolButton
{
public:
    using QToolButton::QToolButton;
    ~UndockButton() override { finishMove(true); }
    std::function<void(QWidget *,const QPoint &)> contextMenu;
protected:
    void contextMenuEvent(QContextMenuEvent *event) override {
        if(!contextMenu) { QToolButton::contextMenuEvent(event); return; }
        m_moveCandidate=false; finishMove(true); setDown(false);
        contextMenu(this,event->globalPos()); event->accept();
    }
    void mousePressEvent(QMouseEvent *event) override
    {
        m_dragged = false;
        m_moveCandidate = event->button() == Qt::LeftButton && isChecked();
        m_pressPosition = event->globalPos();
        QToolButton::mousePressEvent(event);
    }
    void mouseMoveEvent(QMouseEvent *event) override
    {
        if (m_dragged) {
            if (m_dragHost) m_dragHost->updateButtonMove();
            event->accept();
            return;
        }
        if (m_moveCandidate && (event->buttons() & Qt::LeftButton)
            && (event->globalPos() - m_pressPosition).manhattanLength() >= QApplication::startDragDistance()) {
            m_moveCandidate = false;
            if (auto host = dynamic_cast<PaneFloatingWindow *>(window())) {
                // Keep delivery in Qt, even for Resolve's native child headers.
                // Release after a drag must never emit clicked()/toggle docking.
                m_dragged = true;
                setDown(false);
                if (host->beginButtonMove()) {
                    m_dragHost = host;
                    grabMouse();
                    qApp->installEventFilter(this);
                }
                event->accept();
                return;
            }
        }
        QToolButton::mouseMoveEvent(event);
    }
    void mouseReleaseEvent(QMouseEvent *event) override
    {
        m_moveCandidate = false;
        if (m_dragged) { finishMove(false); setDown(false); event->accept(); return; }
        QToolButton::mouseReleaseEvent(event);
    }
    bool event(QEvent *event) override
    {
        if (event->type() == QEvent::Hide || event->type() == QEvent::UngrabMouse) {
            m_moveCandidate = false;
            setDown(false);
            finishMove(true);
        }
        return QToolButton::event(event);
    }
    bool eventFilter(QObject *, QEvent *event) override
    {
        if (m_dragHost && event->type() == QEvent::KeyPress
            && static_cast<QKeyEvent *>(event)->key() == Qt::Key_Escape) {
            finishMove(true);
            return true;
        }
        if (event->type() == QEvent::ApplicationDeactivate) finishMove(true);
        return false;
    }
    void paintEvent(QPaintEvent *) override
    {
        QPainter painter(this);
        icon().paint(&painter,rect(),Qt::AlignCenter,
            !isEnabled() ? QIcon::Disabled : underMouse() ? QIcon::Active : QIcon::Normal,
            isChecked() ? QIcon::On : QIcon::Off);
    }
    void enterEvent(QEvent *event) override { QToolButton::enterEvent(event); update(); }
    void leaveEvent(QEvent *event) override { QToolButton::leaveEvent(event); update(); }
private:
    void finishMove(bool cancel)
    {
        const auto host = m_dragHost;
        m_dragHost = nullptr;
        if (qApp) qApp->removeEventFilter(this);
        if (host) host->finishButtonMove(cancel);
        if (QWidget::mouseGrabber() == this) releaseMouse();
    }
    QPointer<PaneFloatingWindow> m_dragHost;
    QPoint m_pressPosition;
    bool m_moveCandidate = false;
    bool m_dragged = false;
};
class PaneDecoration : public QObject
{
public:
    PaneDecoration(QWidget *pane,QObject *owner,std::function<void()> action,
        std::function<void(QWidget *,const QPoint &)> contextMenu={})
        : QObject(owner),m_pane(pane),m_margins(pane->contentsMargins()),m_action(std::move(action)),m_contextMenu(std::move(contextMenu))
    {
        pane->installEventFilter(this);
        refresh();
    }
    ~PaneDecoration() override
    {
        qDeleteAll(m_headerDrags);
        if(m_pane) {m_pane->removeEventFilter(this); removeStrip();}
        for(auto button:m_buttons) delete button.data();
    }
    QWidget *pane() const { return m_pane; }
    QToolButton *button() const { for(auto b:m_buttons) if(b) return b; return nullptr; }
    bool usesNativeHeader() const { return m_native; }
    void setFloating(bool floating)
    {
        m_floating=floating;
        if(!floating) refresh();
        for(auto b:m_buttons) if(b) {
            b->setChecked(floating);
            b->setToolTip(floating ? "Click to dock; drag to move" : "Undock");
            b->setAccessibleName(floating ? "Dock" : "Undock");
            b->show();
        }
        position();
    }
protected:
    bool eventFilter(QObject *,QEvent *event) override
    {
        if(event->type()==QEvent::Resize || event->type()==QEvent::Show || event->type()==QEvent::LayoutRequest) {
            if(!m_floating) refresh();
            position();
        }
        return false;
    }
private:
    bool requiresNativeHeader() const
    {
        const auto name=m_pane->objectName();
        return name.startsWith("frameAudioPanel") || name=="frameTimelineMixerContainer"
            || name=="frameMetadataContainer" || name=="frameInspectorContainer";
    }
    bool isAnchor(QAbstractButton *b) const
    {
        auto pane=m_pane->objectName();
        const auto name=b->objectName();
        const auto parent=b->parentWidget() ? b->parentWidget()->objectName() : QString();
        if(pane=="frameAudioPanelGroupsContainer") return name=="m_BtnMenu" && parent=="frameHeaderTitle";
        if(pane=="frameAudioPanelAdrContainer") return name=="optionsBtn" && parent=="headerFrame";
        if(pane=="frameAudioPanelMediaPoolContainer") pane="frameEditPageMediaPool";
        if(pane=="frameAudioPanelEffectsLibraryContainer") pane="frameEditPageEffectsLibrary";
        if(pane=="frameAudioPanelIndexContainer") pane="frameEditPageEditIndex";
        if(pane=="frameAudioPanelSoundLibraryContainer") pane="frameEditPageSoundLibrary";
        if(pane=="frameTimelineMixerContainer") pane="frameEditAudioMeter";
        if(pane=="frameMetadataContainer") pane="frameEditPageMetadata";
        if(pane=="frameInspectorContainer") pane="frameEditPageInspector";
        if(pane=="frameEditPageMediaPool" || pane=="m_pWidgetMediaPool") return name=="btnMediaPoolOptions" && parent=="frameMediaPoolTitleBarTop";
        if(pane=="frameEditPageInspector") return name=="btnTitleOptions" && parent=="frameEffectsWidgetsTitle";
        if(pane=="frameEditAudioMeter") return name=="btnAudioMixerOptions" && (parent=="frameOptions" || parent=="frameHeaderContainer");
        if(pane=="frameEditPageEffectsLibrary") return name=="btnOptions" && parent=="frameEffectTopBar";
        if(pane=="frameEditPageSoundLibrary") return name=="m_BtnDisplayOptions" && parent=="frameMenu";
        if(pane=="frameEditPageKeyframeEditor") return name=="btnCurveSettingsOptions" && parent=="keyframeToolbarButtons";
        if(pane=="frameEditPageEditIndex") return (name=="btnEdlOptions" && parent=="frameEDLTopBar")
            || (name=="m_BtnMenu" && (parent=="frameTitlebar" || parent=="frameHeaderTitle"));
        if(pane=="frameEditPageMetadata") return name=="btnPresetsSelector" && parent=="frameMetadataTop";
        if(pane=="m_pWidgetLutBrowser") return name=="btnOptions" && parent=="frameLutBrowserTitleBarTop";
        if(pane=="m_pWidgetGallery") return name=="btnOptions" && parent=="frameTopColorGallery";
        if(pane=="m_pFrameNodeGraph") return name=="btnOptions" && parent=="rightFrame"
            && b->parentWidget()->parentWidget() && b->parentWidget()->parentWidget()->objectName()=="m_NGHeaderFrame";
        return false;
    }
    QToolButton *makeButton(QWidget *parent)
    {
        auto b=new UndockButton(parent); b->setObjectName("resolvePaneUndockButton");
        b->contextMenu=m_contextMenu;
        b->setIcon(QIcon(new UndockIconEngine)); b->setIconSize(QSize(20,20));
        b->setCheckable(true); b->setChecked(m_floating);
        b->setToolButtonStyle(Qt::ToolButtonIconOnly); b->setAccessibleName(m_floating ? "Dock" : "Undock");
        b->setToolTip(m_floating ? "Click to dock; drag to move" : "Undock"); b->setFocusPolicy(Qt::NoFocus);
        b->setFixedSize(20,20);
        b->setStyleSheet("QToolButton { background: transparent; border: none; padding: 0px; }");
        connect(b,&QToolButton::clicked,this,[this] {m_action();});
        m_buttons.append(b); return b;
    }
    void removeStrip()
    {
        if(m_strip && m_pane && m_pane->contentsMargins()==QMargins(m_margins.left(),m_margins.top()+m_strip,m_margins.right(),m_margins.bottom()))
            m_pane->setContentsMargins(m_margins);
        m_strip=0;
    }
    void refresh()
    {
        if(m_refreshing || !m_pane || m_floating) return;
        m_refreshing=true;
        bool any=false; for(auto b:m_buttons) if(b) any=true;
        if(m_native && any) {m_refreshing=false; return;}
        QList<QAbstractButton *> anchors;
        for(auto b:m_pane->findChildren<QAbstractButton *>()) {
            if(!isAnchor(b)) continue;
            auto layout=qobject_cast<QBoxLayout *>(b->parentWidget()->layout());
            if(layout && layout->direction()==QBoxLayout::LeftToRight && layout->indexOf(b)>=0) anchors.append(b);
        }
        if(!anchors.isEmpty()) {
            qDeleteAll(m_headerDrags); m_headerDrags.clear();
            for(auto b:m_buttons) delete b.data(); m_buttons.clear(); removeStrip(); m_native=true;
            for(auto anchor:anchors) {
                auto layout=static_cast<QBoxLayout *>(anchor->parentWidget()->layout());
                auto b=makeButton(anchor->parentWidget());
                layout->insertWidget(layout->indexOf(anchor),b,0,Qt::AlignVCenter); b->show();
                auto header = anchor->parentWidget();
                if ((header->objectName()==QLatin1String("frameOptions")
                        && header->parentWidget()->objectName()==QLatin1String("frameMixerMeterTitle"))
                    || (header->objectName()==QLatin1String("rightFrame")
                        && header->parentWidget()->objectName()==QLatin1String("m_NGHeaderFrame")))
                    header = header->parentWidget();
                m_headerDrags.append(new PaneHeaderDrag(header, this));
            }
        } else if(!any && !requiresNativeHeader()) {
            m_native=false; m_buttons.clear(); m_strip=24;
            m_pane->setContentsMargins(m_margins.left(),m_margins.top()+m_strip,m_margins.right(),m_margins.bottom());
            makeButton(m_pane)->show();
            m_headerDrags.append(new PaneHeaderDrag(m_pane, this, m_strip, m_margins.top()));
        }
        position(); m_refreshing=false;
    }
    void position()
    {
        if(m_native || !m_pane || !button()) return;
        button()->move(qMax(m_margins.left(),m_pane->width()-m_margins.right()-22),m_margins.top()+2);
        button()->raise();
    }
    QPointer<QWidget> m_pane;
    QList<QPointer<QToolButton>> m_buttons;
    QList<PaneHeaderDrag *> m_headerDrags;
    QMargins m_margins;
    std::function<void()> m_action;
    std::function<void(QWidget *,const QPoint &)> m_contextMenu;
    int m_strip=0;
    bool m_native=false,m_floating=false,m_refreshing=false;
};
}
#endif
