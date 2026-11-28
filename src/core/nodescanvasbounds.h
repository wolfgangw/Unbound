// SPDX-License-Identifier: MIT
#ifndef UNBOUND_NODESCANVASBOUNDS_H
#define UNBOUND_NODESCANVASBOUNDS_H
#include <QGraphicsView>
#include <QGraphicsScene>
#include <QScrollBar>
#include <QPointer>
#include <QTimer>
#include <QEvent>
#include <QResizeEvent>
#include <QApplication>
namespace Unbound {
// Resolve's fixed scene rectangle can become smaller than a floating viewport.
// Items outside it still paint, but Resolve rejects dragging them. Expand the
// actual scene coordinates; do not scale or reposition the user's nodes.
class NodesCanvasBounds : public QObject
{
public:
    explicit NodesCanvasBounds(QWidget *content,QObject *parent=nullptr) : QObject(parent),m_content(content)
    {
        m_coalesce.setSingleShot(true);
        m_coalesce.setInterval(50);
        connect(&m_coalesce,&QTimer::timeout,this,[this] { updateBounds(); });
        if (m_content) m_content->installEventFilter(this);
        attachView();
        QTimer::singleShot(0,this,[this] { updateBounds(); });
    }
    ~NodesCanvasBounds() override
    {
        m_coalesce.stop();
        if (m_content) m_content->removeEventFilter(this);
        if (m_view) {
            m_view->removeEventFilter(this);
            if (auto viewport=m_view->viewport()) viewport->removeEventFilter(this);
        }
        for (const auto &connection : m_viewConnections) disconnect(connection);
        disconnect(m_sceneConnection);
    }
    void updateBounds()
    {
        if (m_updating || !m_content || !m_content->isVisible()
            || QApplication::mouseButtons()!=Qt::NoButton) return;
        attachView();
        if (!m_view || !m_view->isVisible() || !m_view->scene() || m_view->viewport()->size().isEmpty()) return;
        auto scene=m_view->scene();
        attachScene(scene);
        const auto visible=m_view->mapToScene(m_view->viewport()->rect()).boundingRect();
        const auto previous=scene->sceneRect();
        if (!visible.isValid() || previous.adjusted(-1,-1,1,1).contains(visible)) return;
        m_updating=true;
        // Allow for a viewport growing when Qt hides its scrollbars, plus
        // integer rounding at fractional zoom scales.
        const int extraX=qMax(0,m_view->width()-m_view->viewport()->width());
        const int extraY=qMax(0,m_view->height()-m_view->viewport()->height());
        const auto padded=m_view->mapToScene(m_view->viewport()->rect().adjusted(
            -extraX/2-2,-extraY/2-2,(extraX+1)/2+2,(extraY+1)/2+2)).boundingRect();
        scene->setSceneRect(previous.united(padded));
        m_view->centerOn(visible.center());
        // Resolve positions its input/output markers in its resize handler.
        // Refresh that calculation after changing the scene's coordinate bounds.
        // QAbstractScrollArea dispatches its virtual resizeEvent through
        // viewportEvent. Sending this to the outer view bypasses Resolve's
        // graph resize handler and leaves the endpoint positions stale.
        QResizeEvent resize(m_view->viewport()->size(),m_view->viewport()->size());
        QCoreApplication::sendEvent(m_view->viewport(),&resize);
        m_updating=false;
    }
protected:
    bool eventFilter(QObject *object,QEvent *event) override
    {
        if (m_updating) return false;
        switch (event->type()) {
        case QEvent::ChildAdded:
            if (object==m_content && !m_view) schedule();
            break;
        case QEvent::Resize:
        case QEvent::Show:
        case QEvent::LayoutRequest:
        case QEvent::Wheel:
        case QEvent::MouseButtonRelease:
        case QEvent::KeyRelease:
            schedule();
            break;
        default: break;
        }
        return false;
    }
private:
    void schedule()
    {
        if (!m_updating && m_content && m_content->isVisible() && !m_coalesce.isActive())
            m_coalesce.start();
    }
    void attachView()
    {
        if (m_view || !m_content) return;
        m_view=m_content->findChild<QGraphicsView *>(QStringLiteral("m_NodeGraphWgt"));
        if (!m_view) return;
        m_view->installEventFilter(this);
        if (auto viewport=m_view->viewport()) viewport->installEventFilter(this);
        for (auto bar : {m_view->horizontalScrollBar(), m_view->verticalScrollBar()}) {
            if (!bar) continue;
            m_viewConnections.append(connect(bar,&QAbstractSlider::rangeChanged,this,[this] { schedule(); }));
            m_viewConnections.append(connect(bar,&QAbstractSlider::valueChanged,this,[this] { schedule(); }));
        }
        attachScene(m_view->scene());
    }
    void attachScene(QGraphicsScene *scene)
    {
        if (m_scene==scene) return;
        disconnect(m_sceneConnection);
        m_scene=scene;
        if (m_scene)
            m_sceneConnection=connect(m_scene,&QGraphicsScene::sceneRectChanged,this,[this] { schedule(); });
    }
    QPointer<QWidget> m_content;
    QPointer<QGraphicsView> m_view;
    QPointer<QGraphicsScene> m_scene;
    QList<QMetaObject::Connection> m_viewConnections;
    QMetaObject::Connection m_sceneConnection;
    QTimer m_coalesce;
    bool m_updating=false;
};
}
#endif
