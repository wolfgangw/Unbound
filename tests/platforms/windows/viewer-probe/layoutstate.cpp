// SPDX-License-Identifier: MIT
// On-demand metadata only: no connection dumps, log-handler replacement,
// property getters, native slot invocation, timers, or widget modification.
#include <QApplication>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QMetaMethod>
#include <QMetaEnum>
#include <QSet>
#include <QThread>
#include <QWidget>
#include <qt_windows.h>

static QString id(const QObject *o) { return QString::number(quintptr(o),16); }
extern "C" __declspec(dllexport) DWORD WINAPI UnboundStart(void *)
{
    auto app=qobject_cast<QApplication *>(QCoreApplication::instance());
    if (!app || QCoreApplication::closingDown()) return 0;
    return QMetaObject::invokeMethod(app,[] {
        QSet<QObject *> seen; QList<QObject *> pending{qApp};
        for (auto w:QApplication::allWidgets()) pending.append(w);
        QJsonArray objects;
        while (!pending.isEmpty() && seen.size()<100000) {
            auto o=pending.takeLast();
            if (seen.contains(o) || o->thread()!=qApp->thread()) continue;
            seen.insert(o);
            // Widgets are already roots. Do not descend through arbitrary
            // models/controllers: a Resolve session can own >100,000 QObjects.
            for (auto child:o->children()) {
                const QByteArray childClass=child->metaObject()->className();
                if (!child->isWidgetType() && (childClass.contains("Layout") ||
                    childClass=="UiResizeManager" || childClass=="UiMultiScreenManager" ||
                    childClass=="UiMainMenuSlots")) pending.append(child);
            }
            const QByteArray cls=o->metaObject()->className();
            if (o->isWidgetType() || !cls.startsWith("Ui")) continue;
            QJsonObject object{{"id",id(o)},{"parent",id(o->parent())},
                {"name",o->objectName()},{"class",QString::fromLatin1(cls)}};
            if (cls.contains("Layout") || cls.contains("Screen") || cls=="UiResizeManager" || cls=="UiMainMenuSlots") {
                QJsonArray methods,enums;
                auto meta=o->metaObject();
                for (int i=QObject::staticMetaObject.methodCount();i<meta->methodCount();++i) {
                    auto m=meta->method(i);
                    methods.append(QJsonObject{{"signature",QString::fromLatin1(m.methodSignature())},{"type",int(m.methodType())}});
                }
                for (int i=0;i<meta->enumeratorCount();++i) {
                    auto e=meta->enumerator(i); QJsonObject values;
                    for (int k=0;k<e.keyCount();++k) values.insert(e.key(k),e.value(k));
                    enums.append(QJsonObject{{"name",e.name()},{"values",values}});
                }
                object.insert("methods",methods); object.insert("enums",enums);
            }
            objects.append(object);
        }
        HMODULE module=nullptr;
        GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS|GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
            reinterpret_cast<LPCWSTR>(&UnboundStart),&module);
        wchar_t path[32768]; const auto length=GetModuleFileNameW(module,path,32768);
        const auto directory=QString::fromWCharArray(path,int(length)).section('\\',0,-2);
        QFile output(QDir(directory).filePath(QStringLiteral("layout-state-%1-%2.json").arg(GetCurrentProcessId()).arg(QDateTime::currentMSecsSinceEpoch())));
        if (output.open(QIODevice::WriteOnly)) output.write(QJsonDocument(QJsonObject{
            {"objects",objects},{"inspected",seen.size()},{"truncated",!pending.isEmpty()}}).toJson());
    },Qt::QueuedConnection) ? 1 : 0;
}
