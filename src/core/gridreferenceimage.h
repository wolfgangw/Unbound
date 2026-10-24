// SPDX-License-Identifier: MIT
#pragma once
#include <QImage>
#include <QRect>
#include <QVector>
#include <array>
#include <cmath>
namespace Unbound {
// Analytic distance to a rounded rectangle gives a smooth Gaussian profile.
// Maximum alpha combines silhouettes without additive brightness at overlaps.
inline QImage gridReferenceImage(const QVector<QRect> &rectangles,QRect area,qreal scale) {
    if(area.isEmpty() || scale<=0)return {};
    QImage image(QSize(int(std::ceil(area.width()*scale)),int(std::ceil(area.height()*scale))),QImage::Format_ARGB32_Premultiplied);
    if(image.isNull())return {};
    image.setDevicePixelRatio(scale);image.fill(Qt::transparent);
    constexpr double extent=6.;
    static const auto alpha=[] {
        constexpr double sigma=2.;
        std::array<int,769> values{};
        for(int i=0;i<int(values.size());++i) {const double d=i/128.;values[i]=qRound(164.5*std::exp(-d*d/(2.*sigma*sigma)));}
        return values;
    }();
    for(auto saved:rectangles) {
        if(saved.isEmpty())continue;
        const QRectF rect(saved.translated(-area.topLeft()));
        const double radius=qMin(10.,qMin(rect.width(),rect.height())/2.);
        const double cx=rect.center().x(),cy=rect.center().y();
        const double hx=rect.width()/2.-radius,hy=rect.height()/2.-radius;
        const int x0=qMax(0,int(std::floor((rect.left()-extent)*scale)));
        const int x1=qMin(image.width(),int(std::ceil((rect.right()+extent)*scale)));
        const int y0=qMax(0,int(std::floor((rect.top()-extent)*scale)));
        const int y1=qMin(image.height(),int(std::ceil((rect.bottom()+extent)*scale)));
        if(x0>=x1 || y0>=y1)continue;
        for(int y=y0;y<y1;++y) {
            auto row=reinterpret_cast<QRgb *>(image.scanLine(y));
            const double py=(y+.5)/scale;
            auto span=[&](int first,int last) {
                for(int x=first;x<last;++x) {
                    const double qx=std::abs((x+.5)/scale-cx)-hx,qy=std::abs(py-cy)-hy;
                    const double ox=qMax(0.,qx),oy=qMax(0.,qy);
                    const double outside=ox && oy ? std::hypot(ox,oy) : ox+oy;
                    const double distance=std::abs(outside+qMin(qMax(qx,qy),0.)-radius);
                    if(distance>=extent)continue;
                    const int opacity=alpha[qBound(0,qRound(distance*128),768)];
                    if(opacity>qAlpha(row[x]))row[x]=qPremultiply(qRgba(237,75,63,opacity));
                }
            };
            // Skip the large empty interior; work is proportional to perimeter.
            if(py<rect.top()+radius || py>rect.bottom()-radius)span(x0,x1);
            else {
                const int left=qBound(x0,int(std::ceil((rect.left()+extent)*scale)),x1);
                const int right=qBound(left,int(std::floor((rect.right()-extent)*scale)),x1);
                span(x0,left);span(right,x1);
            }
        }
    }
    return image;
}
}
