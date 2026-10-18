// SPDX-License-Identifier: MIT
#ifndef UNBOUND_GEOMETRYJSON_H
#define UNBOUND_GEOMETRYJSON_H
#include <QDataStream>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QRect>
#include <cmath>
namespace Unbound {
inline QJsonObject rectangleJson(const QRect &r)
{ return {{"x",r.x()},{"y",r.y()},{"width",r.width()},{"height",r.height()}}; }
inline bool readRectangle(const QJsonValue &value,QRect &r)
{
    const auto o=value.toObject();
    for (auto key : {"x","y","width","height"}) {
        const auto v=o.value(key);
        if (!v.isDouble() || !std::isfinite(v.toDouble()) || std::floor(v.toDouble())!=v.toDouble()
            || qAbs(v.toDouble())>16777215) return false;
    }
    r=QRect(o.value("x").toInt(),o.value("y").toInt(),o.value("width").toInt(),o.value("height").toInt());
    return r.width()>0 && r.height()>0;
}
// Qt's geometry transport stays in memory; native adapters can use its fields.
// The settings file contains only named JSON fields, never encoded blobs.
inline QJsonObject qtGeometryJson(const QByteArray &bytes)
{
    QDataStream s(bytes); s.setVersion(QDataStream::Qt_4_0);
    quint32 magic; quint16 major,minor; QRect frame,normal,client; qint32 screen,width=0; quint8 maximized,full;
    s>>magic>>major>>minor;
    if (s.status()!=QDataStream::Ok || magic!=0x1D9D0CB || major<1 || major>3) return {};
    s>>frame>>normal>>screen>>maximized>>full;
    if (major>1) s>>width;
    if (major>2) s>>client; else client=normal;
    if (s.status()!=QDataStream::Ok) return {};
    return {{"clientRect",rectangleJson(client)},{"normalClientRect",rectangleJson(normal)},
        {"frameRect",rectangleJson(frame)},{"screenIndex",screen},{"screenWidth",width},
        {"maximized",bool(maximized)},{"fullScreen",bool(full)}};
}
inline QByteArray qtGeometryBytes(const QJsonObject &o)
{
    QRect frame,normal,client;
    if (!readRectangle(o.value("clientRect"),client)) return {};
    if (!readRectangle(o.value("normalClientRect"),normal)) normal=client;
    if (!readRectangle(o.value("frameRect"),frame)) frame=client;
    QByteArray bytes; QDataStream s(&bytes,QIODevice::WriteOnly); s.setVersion(QDataStream::Qt_4_0);
    s<<quint32(0x1D9D0CB)<<quint16(3)<<quint16(0)<<frame<<normal
        <<qint32(qMax(0,o.value("screenIndex").toInt()))<<quint8(o.value("maximized").toBool()?2:0)
        <<quint8(o.value("fullScreen").toBool()?4:0)<<qint32(qMax(0,o.value("screenWidth").toInt()))<<client;
    return bytes;
}
inline QJsonObject placementJson(const QByteArray &bytes)
{
    const auto old=QJsonDocument::fromJson(bytes).object();
    if (old.value("format").toString()!=QLatin1String("unbound-main-placement-1")) return qtGeometryJson(bytes);
    const auto a=old.value("normal").toArray();
    if (a.size()!=4) return {};
    QJsonArray monitors;
    for (auto value : old.value("screens").toArray()) {
        const auto v=value.toArray(); if(v.size()!=6) return {};
        monitors.append(QJsonObject{{"name",v[0]},{"workArea",rectangleJson(QRect(v[1].toInt(),v[2].toInt(),v[3].toInt(),v[4].toInt()))},
            {"scaleFactor",v[5]}});
    }
    return {{"normalRect",rectangleJson(QRect(a[0].toInt(),a[1].toInt(),a[2].toInt()-a[0].toInt(),a[3].toInt()-a[1].toInt()))},
        {"coordinateSpace","windows-workspace"},{"maximized",old.value("maximized")},{"monitors",monitors},
        {"qtFallback",qtGeometryJson(QByteArray::fromBase64(old.value("qt").toString().toLatin1()))}};
}
inline QByteArray placementBytes(const QJsonObject &o)
{
    if (!o.contains("normalRect")) return qtGeometryBytes(o);
    QRect r; if(!readRectangle(o.value("normalRect"),r)) return {};
    QJsonArray screens;
    for(auto value:o.value("monitors").toArray()) {
        const auto m=value.toObject(); QRect a;
        if(!readRectangle(m.value("workArea"),a)) return {};
        screens.append(QJsonArray{m.value("name"),a.x(),a.y(),a.width(),a.height(),m.value("scaleFactor")});
    }
    return QJsonDocument(QJsonObject{{"format","unbound-main-placement-1"},
        {"normal",QJsonArray{r.x(),r.y(),r.x()+r.width(),r.y()+r.height()}},{"maximized",o.value("maximized")},
        {"screens",screens},{"qt",QString::fromLatin1(qtGeometryBytes(o.value("qtFallback").toObject()).toBase64())}}).toJson(QJsonDocument::Compact);
}
}
#endif
