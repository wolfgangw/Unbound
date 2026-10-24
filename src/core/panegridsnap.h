// SPDX-License-Identifier: MIT
#pragma once
#include "panesnap.h"
#include <cmath>
namespace Unbound {
// Work-area anchored grid, in the caller's coordinate system. Moving aligns the
// leading edges, preserving size; resizing aligns only the manipulated edges.
// A fixed moving anchor avoids alternating short steps for off-grid window sizes.
inline QRect paneGridArea(QPoint cursor,const QVector<QRect> &areas) {
    QRect result; qint64 best=std::numeric_limits<qint64>::max();
    for(const auto &area:areas) {
        if(area.isEmpty())continue;
        const qint64 dx=qMax(0,qMax(area.left()-cursor.x(),cursor.x()-area.right()));
        const qint64 dy=qMax(0,qMax(area.top()-cursor.y(),cursor.y()-area.bottom()));
        if(dx*dx+dy*dy<best) {best=dx*dx+dy*dy;result=area;}
    }
    return result;
}
class PaneGridSnap {
    struct Axis { bool valid=false;int line=0; } x,y;
    QRect area; int spacing=0;
    QPoint changed;
    int axis(Axis &state,int from,int fixed,int origin,int extent,int step,
        bool leading,bool trailing,int minimum,int maximum,int &active,int &retarget) {
        if(!leading && !trailing) {state.valid=false;return 0;}
        const bool move=leading && trailing;
        int lo=origin,hi=origin+extent;
        if(!move) {
            lo=qMax(lo,leading ? fixed-maximum : fixed+minimum);
            hi=qMin(hi,leading ? fixed-minimum : fixed+maximum);
        }
        const int first=int(std::ceil(double(lo-origin)/step));
        const int last=int(std::floor(double(hi-origin)/step));
        if(first>last) {state.valid=false;return 0;}
        int line=origin+qBound(first,qRound(double(from-origin)/step),last)*step;
        // Two logical pixels around the midpoint prevent boundary jitter.
        if(state.valid && state.line>=lo && state.line<=hi &&
            qAbs(from-state.line)<=step/2+qMax(1,step/12))line=state.line;
        retarget=retarget || !state.valid || line!=state.line;
        state.valid=true;state.line=line;active=1;
        return line-from;
    }
public:
    void reset() {x={};y={};area={};spacing=0;changed={};}
    QPoint takeChanged() {const auto value=changed;changed={};return value;}
    QPoint offset(const QRect &rect,const QRect &work,int step,bool left,bool right,
        bool top,bool bottom,QSize minimum,QSize maximum,QPoint *engaged=nullptr) {
        if(engaged)*engaged={};
        if(work.isEmpty() || step<=0) {reset();return {};}
        if(work!=area || step!=spacing) {reset();area=work;spacing=step;}
        int ax=0,ay=0,cx=changed.x(),cy=changed.y();
        const int dx=axis(x,left ? rect.x():rect.x()+rect.width(),
            left ? rect.x()+rect.width():rect.x(),work.x(),work.width(),step,left,right,
            qMax(1,minimum.width()),maximum.width(),ax,cx);
        const int dy=axis(y,top ? rect.y():rect.y()+rect.height(),
            top ? rect.y()+rect.height():rect.y(),work.y(),work.height(),step,top,bottom,
            qMax(1,minimum.height()),maximum.height(),ay,cy);
        changed={cx,cy};if(engaged)*engaged={ax,ay};return {dx,dy};
    }
};
}
