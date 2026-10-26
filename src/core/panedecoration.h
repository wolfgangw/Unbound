// SPDX-License-Identifier: MIT
#ifndef UNBOUND_PANEDECORATION_H
#define UNBOUND_PANEDECORATION_H
#include "panecatalogue.h"
#include "colornativepalette.h"
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
#include <QVariantAnimation>
#include "windowintegration.h"
#include "paneheaderdrag.h"
#include <functional>
namespace Unbound {
// Proposal 5, rendered as vectors without adding a QtSvg runtime dependency.
class UndockIconEngine : public QIconEngine
{
public:
    explicit UndockIconEngine(QColor color={}) : m_color(color) {}
    QIconEngine *clone() const override { return new UndockIconEngine(m_color); }
    void paint(QPainter *p,const QRect &r,QIcon::Mode mode,QIcon::State) override
    {
        p->save(); p->setRenderHint(QPainter::Antialiasing);
        const qreal scale=qMin(r.width(),r.height())/20.0;
        p->translate(r.x()+(r.width()-20*scale)/2,r.y()+(r.height()-20*scale)/2); p->scale(scale,scale);
        const int brightness=mode==QIcon::Disabled ? 90 : mode==QIcon::Active ? 220 : 132;
        p->setPen(QPen(m_color.isValid() ? m_color : QColor(brightness,brightness,brightness),1.5,Qt::SolidLine,Qt::FlatCap,Qt::MiterJoin));
        p->setBrush(Qt::NoBrush);
        QPainterPath large; large.moveTo(15.5,7); large.lineTo(15.5,4.75); large.quadTo(15.5,3.5,14.25,3.5);
        large.lineTo(3.75,3.5); large.quadTo(2.5,3.5,2.5,4.75); large.lineTo(2.5,15.25);
        large.quadTo(2.5,16.5,3.75,16.5); large.lineTo(14.25,16.5); large.quadTo(15.5,16.5,15.5,15.25); large.lineTo(15.5,13);
        p->drawPath(large); p->drawRoundedRect(QRectF(12.5,7,6,6),.75,.75); p->restore();
    }
    QPixmap pixmap(const QSize &size,QIcon::Mode mode,QIcon::State state) override
    { QPixmap pm(size); pm.fill(Qt::transparent); QPainter p(&pm); paint(&p,QRect(QPoint(),size),mode,state); return pm; }
private:
    QColor m_color;
};
// Paint only the glyph: Resolve's inherited button styles must not add a
// hover/checked background. QToolButton still supplies clicks and tooltips.
class UndockButton : public QToolButton
{
public:
    explicit UndockButton(QWidget *parent=nullptr) : QToolButton(parent) {
        m_pulse.setDuration(1400); m_pulse.setLoopCount(-1);
        m_pulse.setStartValue(0.0); m_pulse.setKeyValueAt(.5,1.0); m_pulse.setEndValue(0.0);
        m_pulse.setEasingCurve(QEasingCurve::InOutSine);
        connect(&m_pulse,&QVariantAnimation::valueChanged,this,[this](const QVariant &value) { m_glow=value.toReal(); update(); });
        connect(qApp,&QGuiApplication::applicationStateChanged,this,[this] { updatePulse(); });
    }
    ~UndockButton() override { finishMove(true); }
    std::function<void(QWidget *,const QPoint &)> contextMenu;
    QString dockingFailure() const { return m_failure; }
    bool dockingWarningPending() const { return !m_failure.isEmpty() && !m_acknowledged; }
    bool dockingWarningAnimating() const { return m_pulse.state()==QAbstractAnimation::Running; }
    void setFloatingState(bool floating) {
        setChecked(floating); setAccessibleName(floating ? "Dock" : "Undock"); updateTooltip();
    }
    void setDockingFailure(const QString &message) {
        m_failure=message; m_acknowledged=false; updateTooltip(); updatePulse(); update();
    }
    void acknowledgeDockingFailure() { m_acknowledged=true; updatePulse(); update(); }
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
        if(event->type()==QEvent::Show || event->type()==QEvent::Hide) updatePulse();
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
        if(!m_failure.isEmpty()) {
            const qreal amount=m_acknowledged ? 0.0 : m_glow;
            const QColor red("#ed4b3f"), muted("#848484");
            const auto mix=[amount](int a,int b) { return qRound(a+(b-a)*amount); };
            const QColor stroke(mix(muted.red(),red.red()),mix(muted.green(),red.green()),mix(muted.blue(),red.blue()));
            QRadialGradient glow(QRectF(rect()).center(),width()*.65);
            QColor halo=red; halo.setAlphaF(.48*amount); glow.setColorAt(0,halo);
            halo.setAlpha(0); glow.setColorAt(1,halo); painter.fillRect(rect(),glow);
            UndockIconEngine glyph(stroke); glyph.paint(&painter,rect(),QIcon::Normal,QIcon::Off);
            return;
        }
        icon().paint(&painter,rect(),Qt::AlignCenter,
            !isEnabled() ? QIcon::Disabled : underMouse() ? QIcon::Active : QIcon::Normal,
            isChecked() ? QIcon::On : QIcon::Off);
    }
    void enterEvent(QEvent *event) override { QToolButton::enterEvent(event); update(); }
    void leaveEvent(QEvent *event) override { QToolButton::leaveEvent(event); update(); }
private:
    void updateTooltip() {
        setToolTip(m_failure.isEmpty() ? (isChecked() ? "Click to dock; drag to move" : "Undock") :
            "DaVinci Resolve couldn't make room for this panel. It remains floating. Right-click for details.");
    }
    void updatePulse() {
        const bool animate=dockingWarningPending() && isVisible() && QGuiApplication::applicationState()==Qt::ApplicationActive;
        if(animate && m_pulse.state()!=QAbstractAnimation::Running) m_pulse.start();
        else if(!animate) { m_pulse.stop(); m_glow=0; }
    }
    QString m_failure;
    bool m_acknowledged=false;
    qreal m_glow=0;
    QVariantAnimation m_pulse;
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
        if(m_gutterContent) {
            m_gutterContent->removeEventFilter(this);
            if(m_gutterContent->contentsMargins()==m_gutterMargins+QMargins(32,0,0,0))
                m_gutterContent->setContentsMargins(m_gutterMargins);
        }
        delete m_gutterDragSurface.data();
        for(auto button:m_buttons) delete button.data();
    }
    QWidget *pane() const { return m_pane; }
    QToolButton *button() const { for(auto b:m_buttons) if(b) return b; return nullptr; }
    bool usesNativeHeader() const { return m_native; }
    void refreshNativeHeader() { if(m_pane && PaneCatalogue::colorControl(m_pane->objectName())) refresh(); }
    void setDockingFailure(const QString &message) {
        m_failure=message;
        for(auto b:m_buttons) if(b) static_cast<UndockButton *>(b.data())->setDockingFailure(message);
    }
    void setFloating(bool floating)
    {
        m_floating=floating;
        if(!floating) refresh();
        for(auto b:m_buttons) if(b) {
            static_cast<UndockButton *>(b.data())->setFloatingState(floating);
            b->show();
        }
        syncMenuButtons();
        position();
    }
protected:
    bool eventFilter(QObject *object,QEvent *event) override
    {
        if(event->type()==QEvent::ShowToParent || event->type()==QEvent::HideToParent)
            for(const auto &pair:m_menuButtons) if(pair.first==object && pair.second)
                pair.second->setVisible(!pair.first->isHidden());
        if(event->type()==QEvent::Resize || event->type()==QEvent::Show || event->type()==QEvent::LayoutRequest) {
            if(!m_floating) refresh();
            position();
        }
        return false;
    }
private:
    bool requiresNativeHeader() const
    {
        const auto pane=PaneCatalogue::find(m_pane->objectName());
        return pane && pane->requireNativeHeader();
    }
    bool isAnchor(QAbstractButton *button) const
    {
        const auto pane=PaneCatalogue::find(m_pane->objectName());
        if(!pane) return false;
        const auto parent=button->parentWidget();
        const auto grandparent=parent ? parent->parentWidget() : nullptr;
        return PaneCatalogue::matchesHeader(pane->header,button->objectName(),
            parent ? parent->objectName() : QString(),grandparent ? grandparent->objectName() : QString());
    }
    QToolButton *makeButton(QWidget *parent)
    {
        auto b=new UndockButton(parent); b->setObjectName("resolvePaneUndockButton");
        b->contextMenu=[this](QWidget *button,const QPoint &position) {
            if(m_contextMenu) m_contextMenu(button,position);
            for(auto b:m_buttons) if(b) static_cast<UndockButton *>(b.data())->acknowledgeDockingFailure();
        };
        b->setIcon(QIcon(new UndockIconEngine)); b->setIconSize(QSize(20,20));
        b->setCheckable(true); b->setChecked(m_floating);
        b->setToolButtonStyle(Qt::ToolButtonIconOnly); b->setAccessibleName(m_floating ? "Dock" : "Undock");
        b->setToolTip(m_floating ? "Click to dock; drag to move" : "Undock"); b->setFocusPolicy(Qt::NoFocus);
        b->setFixedSize(20,20);
        if(!m_failure.isEmpty()) b->setDockingFailure(m_failure);
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
    void syncMenuButtons() {
        for(const auto &pair:m_menuButtons) if(pair.first && pair.second)
            pair.second->setVisible(!pair.first->isHidden());
    }
    QList<QPair<QPointer<QAbstractButton>,QPointer<QToolButton>>> m_menuButtons;
    void addHeaderDrag(QWidget *header) {
        for(const auto &surface:m_controlDragSurfaces) if(surface==header) return;
        m_controlDragSurfaces.append(header);
        m_headerDrags.append(new PaneHeaderDrag(header,this));
    }
    QList<QPointer<QWidget>> m_controlDragSurfaces;
    void refresh()
    {
        if(m_refreshing || !m_pane || (m_floating && !PaneCatalogue::colorControl(m_pane->objectName()))) return;
        m_refreshing=true;
        bool any=false; for(auto b:m_buttons) if(b) any=true;
        if(m_native && any && !PaneCatalogue::colorControl(m_pane->objectName())) {m_refreshing=false; return;}
        const auto paneDescriptor=PaneCatalogue::find(m_pane->objectName());
        if(const auto control=PaneCatalogue::colorControl(m_pane->objectName())) {
            auto root=m_floating && button() ? button()->window() : m_pane.data();
            if(!m_floating && ColorNativePalette::supports(m_pane))
                if(auto palette=ColorNativePalette::palette(m_pane)) root=palette;
            auto add=[this](QWidget *header,QBoxLayout *row,QAbstractButton *anchor,bool mirror=false) {
                // Retain installed rows and add only newly created native variants.
                for(auto b:m_buttons) if(b && b->parentWidget()==header
                    && (!anchor || b->property("unboundAnchor").value<QObject *>()==anchor)) return;
                auto b=makeButton(header); b->setProperty("unboundAnchor",QVariant::fromValue(static_cast<QObject *>(anchor)));
                const bool rtl=row->direction()==QBoxLayout::RightToLeft;
                row->insertWidget(anchor ? row->indexOf(anchor)+(rtl ? 1 : 0) : row->count(),b,0,Qt::AlignVCenter);
                if(mirror) {
                    m_menuButtons.append({anchor,b}); anchor->installEventFilter(this);
                    b->setVisible(!anchor->isHidden());
                } else b->show();
                m_native=true;
            };
            for(auto header:root->findChildren<QWidget *>(control->header)) {
                if(!header->parentWidget() || header->parentWidget()->objectName()!=control->headerOwner) continue;
                auto row=qobject_cast<QBoxLayout *>(header->layout());
                if(!row || (row->direction()!=QBoxLayout::LeftToRight && row->direction()!=QBoxLayout::RightToLeft)) continue;
                bool nested=false;
                for(auto selector:header->findChildren<QWidget *>()) {
                    // Several tools use the native palette selector under
                    // different object names. Its title/background frames are
                    // safe drag roots; selector buttons remain native controls.
                    if(selector->objectName()!=QLatin1String("m_WidgetSelector")
                        && !(control->objectName==QLatin1String("sizingTab")
                            && selector->objectName()==QLatin1String("cbWidgets"))
                        && !(control->objectName==QLatin1String("trackerTab")
                            && selector->objectName()==QLatin1String("cbBoxTrackStableSelection"))
                        && !((control->objectName==QLatin1String("blurTab") || control->objectName==QLatin1String("tab3D"))
                            && selector->objectName()==QLatin1String("cmbSetTo"))
                        && !(control->objectName==QLatin1String("colorWheelsTab")
                            && selector->objectName()==QLatin1String("cbColorWidgets"))) continue;
                    for(auto actions:selector->findChildren<QWidget *>("frameHeaderButtons")) {
                        auto actionsRow=qobject_cast<QBoxLayout *>(actions->layout());
                        if(!actionsRow) continue;
                        const auto anchors=actions->findChildren<QAbstractButton *>(QString(),Qt::FindDirectChildrenOnly);
                        for(auto anchor:anchors) if(QByteArray(anchor->metaObject()->className())=="BmdDavUI::UiMenuButton"
                            && anchor->objectName().isEmpty() && actionsRow->indexOf(anchor)>=0) {
                            add(actions,actionsRow,anchor,true); nested=true;
                        }
                    }
                    // UiPaletteSelector is a native control, not itself a safe
                    // drag target. Its plain child frames use our standard filter.
                    for(auto surface:selector->findChildren<QWidget *>(QString(),Qt::FindDirectChildrenOnly)) {
                        const QByteArray type=surface->metaObject()->className();
                        if(type=="QWidget" || type=="QFrame") addHeaderDrag(surface);
                    }
                }
                if(!nested) {
                    auto anchor=control->anchor.isEmpty() ? nullptr : header->findChild<QAbstractButton *>(control->anchor,Qt::FindDirectChildrenOnly);
                    if(!control->anchor.isEmpty() && (!anchor || row->indexOf(anchor)<0)) continue;
                    add(header,row,anchor);
                }
                addHeaderDrag(header);
            }
            m_refreshing=false; return;
        }
        if(paneDescriptor && paneDescriptor->header==PaneCatalogue::Header::ClipsGutter) {
            if(auto content=m_pane->findChild<QWidget *>("frameLightBoxBorder",Qt::FindDirectChildrenOnly)) {
                m_gutterContent=content; m_gutterMargins=content->contentsMargins();
                content->setContentsMargins(m_gutterMargins+QMargins(32,0,0,0));
                content->installEventFilter(this);
                m_gutterDragSurface=new QWidget(content);
                m_gutterDragSurface->setObjectName("unboundClipsGutter");
                m_gutterDragSurface->setFocusPolicy(Qt::NoFocus);
                m_gutterDragSurface->show();
                m_headerDrags.append(new PaneHeaderDrag(m_gutterDragSurface,this));
                makeButton(content)->show(); m_native=true;
                if(auto header=content->findChild<QWidget *>("topBorderFrame",Qt::FindDirectChildrenOnly))
                    m_headerDrags.append(new PaneHeaderDrag(header,this));
            }
            position(); m_refreshing=false; return;
        }
        if(paneDescriptor && paneDescriptor->header==PaneCatalogue::Header::TimelineCorner) {
            // Timeline has no header. Keep the corner control on the transferred
            // native content, not on its fixed-height slot or on the Clips sibling.
            if(auto content=m_pane->findChild<QWidget *>(QStringLiteral("m_pTimeLinePnl"),Qt::FindDirectChildrenOnly)) {
                makeButton(content)->show(); m_native=true; m_cornerOverlay=true;
            }
            position(); m_refreshing=false; return;
        }
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
                        && header->parentWidget()->objectName()==QLatin1String("m_NGHeaderFrame"))
                    || (header->objectName()==QLatin1String("frameTopRight")
                        && header->parentWidget()->objectName()==QLatin1String("frameOFXTitle")))
                    header = header->parentWidget();
                m_headerDrags.append(new PaneHeaderDrag(header, this));
            }
            const auto descriptor=PaneCatalogue::find(m_pane->objectName());
            if(descriptor && !descriptor->additionalDragHeader.isEmpty())
                if(auto header=m_pane->findChild<QWidget *>(descriptor->additionalDragHeader))
                    m_headerDrags.append(new PaneHeaderDrag(header,this));
            if(descriptor && descriptor->header==PaneCatalogue::Header::ColorViewer) {
                // The viewer's second header row is a stack of native tool
                // pages. Observe each page with the same safe-surface rules;
                // controls retain clicks and only unused background drags.
                if(auto tools=m_pane->findChild<QWidget *>("m_StackedWidgetSubOptions"))
                    for(auto page:tools->findChildren<QWidget *>(QString(),Qt::FindDirectChildrenOnly))
                        m_headerDrags.append(new PaneHeaderDrag(page,this));
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
        if(m_gutterContent && button()) {
            // Match Timeline's centered 32px gutter. In Lightbox the first
            // thumbnail decoration sits below the native filtering/size header.
            int top=m_gutterMargins.top();
            if(auto view=m_gutterContent->findChild<QWidget *>("frameThumbnailView"))
                top=view->mapTo(m_gutterContent,QPoint(0,0)).y();
            if(m_gutterDragSurface) {
                m_gutterDragSurface->setGeometry(m_gutterMargins.left(),0,32,m_gutterContent->height());
                m_gutterDragSurface->raise();
            }
            button()->move(m_gutterMargins.left()+(32-button()->width())/2,top+(m_floating?16:4));
            button()->raise(); return;
        }
        // Center the 20 px control in Timeline's 32 px track-label gutter.
        if(m_cornerOverlay && button()) { button()->move(6,2); button()->raise(); return; }
        if(m_native || !m_pane || !button()) return;
        button()->move(qMax(m_margins.left(),m_pane->width()-m_margins.right()-22),m_margins.top()+2);
        button()->raise();
    }
    QPointer<QWidget> m_pane,m_gutterContent,m_gutterDragSurface;
    QMargins m_gutterMargins;
    QList<QPointer<QToolButton>> m_buttons;
    QList<PaneHeaderDrag *> m_headerDrags;
    QMargins m_margins;
    std::function<void()> m_action;
    std::function<void(QWidget *,const QPoint &)> m_contextMenu;
    int m_strip=0;
    bool m_native=false,m_floating=false,m_refreshing=false;
    bool m_cornerOverlay=false;
    QString m_failure;
};
}
#endif
