// SPDX-License-Identifier: MIT
// Read-only public Qt metadata; no native handlers, getters or UI changes.
#include <QApplication>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QMetaMethod>
#include <QMetaProperty>
#include <QWidget>
#include <qt_windows.h>
extern "C" __declspec(dllexport) DWORD WINAPI UnboundStart(void *) {
    auto app=qobject_cast<QApplication *>(QCoreApplication::instance());
    if(!app || QCoreApplication::closingDown()) return 0;
    return QMetaObject::invokeMethod(app,[] {
        QJsonArray widgets;
        for(auto w:QApplication::allWidgets()) {
            if(w->objectName()!="m_TiTimelineView" && w->objectName()!="UiTiLayoutContainer" &&
                w->objectName()!="timelineViewsSplitter") continue;
            QJsonArray methods,properties;
            for(int i=QWidget::staticMetaObject.methodCount();i<w->metaObject()->methodCount();++i)
                methods.append(QString::fromLatin1(w->metaObject()->method(i).methodSignature()));
            for(int i=QWidget::staticMetaObject.propertyCount();i<w->metaObject()->propertyCount();++i)
                properties.append(QString::fromLatin1(w->metaObject()->property(i).name()));
            widgets.append(QJsonObject{{"name",w->objectName()},{"class",w->metaObject()->className()},
                {"methods",methods},{"properties",properties}});
        }
        HMODULE module=nullptr;
        GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS|GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
            reinterpret_cast<LPCWSTR>(&UnboundStart),&module);
        wchar_t path[32768]; auto n=GetModuleFileNameW(module,path,32768);
        auto directory=QString::fromWCharArray(path,int(n)).section('\\',0,-2);
        QFile file(QDir(directory).filePath(QString("timeline-%1-%2.json").arg(GetCurrentProcessId()).arg(QDateTime::currentMSecsSinceEpoch())));
        if(file.open(QIODevice::WriteOnly)) file.write(QJsonDocument(widgets).toJson());
    },Qt::QueuedConnection) ? 1 : 0;
}
