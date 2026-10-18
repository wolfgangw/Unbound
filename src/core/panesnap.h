// SPDX-License-Identifier: MIT
#ifndef UNBOUND_PANESNAP_H
#define UNBOUND_PANESNAP_H
#include <QRect>
#include <QVector>
#include <limits>
namespace Unbound {
// Shared interaction policy. Backends convert logical distance to native units
// and drive the frame timer using their own event loops.
inline constexpr int PaneSnapDistance=24;
inline constexpr int PaneSnapFrameIntervalMs=16;
inline constexpr int PaneSnapTransitionDurationMs=32;
// Ease only the magnetic correction. Raw pointer movement never trails behind
// the cursor. The caller supplies a monotonic clock and drives frames only
// while transitioning; exact final geometry is committed on release.
class PaneSnapTransition {
    struct Axis {
        bool engaged=false;
        double start=0,current=0;
        qint64 began=-PaneSnapTransitionDurationMs;
        bool pending=false;
        int update(int desired,bool active,qint64 now) {
            if(active!=engaged) { start=current; began=now; engaged=active; }
            const double t=qBound(0.,double(now-began)/PaneSnapTransitionDurationMs,1.);
            // A short accelerating pull: gentle entry, fastest at contact.
            const double eased=t*t;
            current=start+((engaged ? desired : 0)-start)*eased;
            pending=t<1. && (engaged || start!=0.);
            return qRound(current);
        }
    };
    Axis m_x,m_y;
public:
    void reset() { m_x=Axis{}; m_y=Axis{}; }
    QPoint update(QPoint desired,QPoint engaged,qint64 now) {
        return {m_x.update(desired.x(),engaged.x()!=0,now),m_y.update(desired.y(),engaged.y()!=0,now)};
    }
    bool animating() const { return m_x.pending || m_y.pending; }
};
// Native move loops can feed a previously snapped rectangle back to us. Derive
// new proposals from accumulated pointer travel, not that snapped position.
class PaneDragProposal
{
public:
    void reset() { m_valid = false; }
    QRect update(const QRect &suggested, const QPoint &cursor, bool left, bool right, bool top, bool bottom)
    {
        if (!m_valid) { m_start = suggested; m_cursor = cursor; m_valid = true; }
        const auto delta = cursor - m_cursor;
        const int x = m_start.x() + (left ? delta.x() : 0);
        const int y = m_start.y() + (top ? delta.y() : 0);
        return QRect(x, y, m_start.width() + (right ? delta.x() : 0) - (left ? delta.x() : 0),
            m_start.height() + (bottom ? delta.y() : 0) - (top ? delta.y() : 0));
    }
private:
    bool m_valid = false;
    QRect m_start;
    QPoint m_cursor;
};
// Rectangles use screen coordinates; compare exclusive right/bottom edges.
// Window edges align across gaps. Work-area targets stay local to their spans.
inline QPoint paneSnapOffset(const QRect &moving, const QVector<QRect> &targets, int distance,
    bool left = true, bool right = true, bool top = true, bool bottom = true,
    QSize minimum = QSize(), QSize maximum = QSize(16777215, 16777215),
    const QVector<QRect> &workAreas = {}, QPoint *engaged = nullptr)
{
    int bestX = distance + 1, bestY = distance + 1;
    qint64 bestGapX=std::numeric_limits<qint64>::max(),bestGapY=bestGapX;
    auto consider = [distance](int delta, qint64 gap, int &best, qint64 &bestGap) {
        if (qAbs(delta) <= distance && (qAbs(delta) < qAbs(best) ||
            (qAbs(delta)==qAbs(best) && gap<bestGap))) {
            best = delta; bestGap=gap;
        }
    };
    auto target = [&](const QRect &other,bool local) {
        const qint64 dx=qMax(qint64(0),qMax(qint64(other.x())-moving.x()-moving.width(),
            qint64(moving.x())-other.x()-other.width()));
        const qint64 dy=qMax(qint64(0),qMax(qint64(other.y())-moving.y()-moving.height(),
            qint64(moving.y())-other.y()-other.height()));
        // Nearest edge wins; equally close edges favour the nearer window.
        // Identical edge coordinates naturally produce the same result.
        // Windows retain precedence over work areas when edge distances tie.
        const qint64 gap=local ? std::numeric_limits<qint64>::max() : dx*dx+dy*dy;
        if (!local || dy<=distance) {
            for (int edge = 0; edge < 2; ++edge) {
                if (!(edge ? right : left)) continue;
                int from = edge ? moving.x() + moving.width() : moving.x();
                for (int to : {other.x(), other.x() + other.width()}) {
                    const int width = moving.width() + (edge ? to - from : from - to);
                    if (left == right || (width >= minimum.width() && width <= maximum.width())) consider(to - from, gap, bestX, bestGapX);
                }
            }
        }
        if (!local || dx<=distance) {
            for (int edge = 0; edge < 2; ++edge) {
                if (!(edge ? bottom : top)) continue;
                int from = edge ? moving.y() + moving.height() : moving.y();
                for (int to : {other.y(), other.y() + other.height()}) {
                    const int height = moving.height() + (edge ? to - from : from - to);
                    if (top == bottom || (height >= minimum.height() && height <= maximum.height())) consider(to - from, gap, bestY, bestGapY);
                }
            }
        }
    };
    for (const auto &other : targets) target(other,false);
    for (const auto &area : workAreas) target(area,true);
    if(engaged) *engaged=QPoint(qAbs(bestX)<=distance,qAbs(bestY)<=distance);
    return QPoint(qAbs(bestX) <= distance ? bestX : 0, qAbs(bestY) <= distance ? bestY : 0);
}
}
#endif
