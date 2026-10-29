// SPDX-License-Identifier: MIT
#include <QApplication>
#include <QAbstractButton>
#include <QAbstractSlider>
#include <QLabel>
#include <QMetaProperty>
#include <QMetaMethod>
#include <QJsonDocument>
#include <QJsonArray>
#include <QJsonObject>
#include <QDateTime>
#include <QDir>
#include <QSaveFile>
#include <qt_windows.h>
extern "C" __declspec(dllexport) DWORD WINAPI UnboundStart(void *) {
    if(!qApp)return 0;
    return QMetaObject::invokeMethod(qApp,[] {
        QJsonArray rows;
        for(auto w:QApplication::allWidgets()) {
            bool viewer=false;
            for(auto p=w;p;p=p->parentWidget())if(p->objectName()=="m_pFramePlayer"
                || p->objectName()=="unboundFloating_m_pFramePlayer")viewer=true;
            if(!viewer)continue;
            QJsonArray properties,methods;
            for(int i=QWidget::staticMetaObject.propertyCount();i<w->metaObject()->propertyCount();++i) {
                auto p=w->metaObject()->property(i);
                properties.append(QJsonObject{{"name",p.name()},{"type",p.typeName()}});
            }
            for(int i=QWidget::staticMetaObject.methodCount();i<w->metaObject()->methodCount();++i)
                methods.append(QString::fromLatin1(w->metaObject()->method(i).methodSignature()));
            QJsonObject row{{"name",w->objectName()},{"class",w->metaObject()->className()},
                {"properties",properties},{"methods",methods},{"visible",w->isVisible()},{"hidden",w->isHidden()}};
            if(auto label=qobject_cast<QLabel *>(w))row.insert("text",label->text());
            if(auto button=qobject_cast<QAbstractButton *>(w))row.insert("checked",button->isChecked());
            if(auto slider=qobject_cast<QAbstractSlider *>(w))row.insert("value",slider->value());
            rows.append(row);
        }
        auto path=QDir::tempPath()+"/Unbound-color-transport";QDir().mkpath(path);
        QSaveFile file(path+"/"+QDateTime::currentDateTimeUtc().toString("yyyyMMdd-HHmmss-zzz")+".json");
        if(file.open(QIODevice::WriteOnly)){file.write(QJsonDocument(rows).toJson());file.commit();}
    },Qt::QueuedConnection)?1:0;
}
