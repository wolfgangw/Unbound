// SPDX-License-Identifier: MIT
// Observe native anchor signals. The opaque anchor pointer is never dereferenced.
#include <QApplication>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QMetaProperty>
#include <QPointer>
#include <QTimer>
#include <QWidget>
#include <qt_windows.h>

class UiLayoutAnchor;
static QString outputDirectory;
static QString id(const QObject *o) { return QString::number(quintptr(o),16); }
static QJsonArray rect(QRect r) { return {r.x(),r.y(),r.width(),r.height()}; }
class AnchorObserver final : public QObject {
    Q_OBJECT
    QJsonArray samples,connections;
    QList<QMetaObject::Connection> subscriptions;
    QList<QPointer<QWidget>> regions;
    bool stopped=false;
    QJsonArray geometry() const {
        QJsonArray result;
        for (auto w:regions) if (w) result.append(QJsonObject{
            {"id",id(w)},{"name",w->objectName()},{"class",w->metaObject()->className()},
            {"geometry",rect(w->geometry())},
            {"screenGeometry",rect(QRect(w->mapToGlobal(QPoint()),w->size()))},
            {"minimum",QJsonArray{w->minimumWidth(),w->minimumHeight()}},
            {"maximum",QJsonArray{w->maximumWidth(),w->maximumHeight()}},
            {"visible",w->isVisible()}});
        return result;
    }
    void write(const QString &phase) {
        QFile file(QDir(outputDirectory).filePath(QStringLiteral("anchors-%1-%2-%3.json")
            .arg(GetCurrentProcessId()).arg(phase).arg(QDateTime::currentMSecsSinceEpoch())));
        if (file.open(QIODevice::WriteOnly)) file.write(QJsonDocument(QJsonObject{
            {"connections",connections},{"samples",samples},{"regions",geometry()}}).toJson());
    }
public:
    explicit AnchorObserver(QObject *parent):QObject(parent) {
        QWidget *edit=nullptr;
        for (auto w:QApplication::allWidgets()) if (w->objectName()=="m_pConformPanel") {edit=w;break;}
        if (edit) {
            for (auto child:edit->children()) {
                if (QByteArray(child->metaObject()->className())!="UiLayout::UiDynamicLayout") continue;
                const auto c=connect(child,SIGNAL(AnchorMoved(UiLayoutAnchor*,int)),
                    this,SLOT(anchorMoved(UiLayoutAnchor*,int)),Qt::DirectConnection);
                subscriptions.append(c);
                QJsonArray properties;
                for (int i=0;i<child->metaObject()->propertyCount();++i)
                    properties.append(QString::fromLatin1(child->metaObject()->property(i).name()));
                connections.append(QJsonObject{{"sender",id(child)},{"connected",bool(c)},
                    {"properties",properties},{"dynamicProperties",QJsonArray::fromStringList([&]{
                        QStringList names; for (auto n:child->dynamicPropertyNames()) names.append(QString::fromLatin1(n));return names;}())}});
            }
            for (auto w:edit->findChildren<QWidget *>()) if (w->objectName()=="frameEditPageWidgetsContainer" ||
                w->objectName()=="frameEditPageVideoPlayers" || w->objectName()=="frameEditTimelinesHolder" ||
                w->objectName()=="timelineBarAnchor_Splitter") regions.append(w);
        }
        write("start");
        QTimer::singleShot(120000,this,[this]{finish();});
    }
    void finish() {
        if (stopped) return;
        stopped=true;
        // Disconnect our incoming signal connections only.
        for (const auto &connection:subscriptions) disconnect(connection);
        write("stop");deleteLater();
    }
private slots:
    void anchorMoved(UiLayoutAnchor *anchor,int position) {
        if (stopped || samples.size()>=100) return;
        samples.append(QJsonObject{{"sender",id(sender())},
            {"anchor",QString::number(quintptr(anchor),16)},{"position",position},{"regions",geometry()}});
    }
};
extern "C" __declspec(dllexport) DWORD WINAPI UnboundStart(void *)
{
    auto app=qobject_cast<QApplication *>(QCoreApplication::instance());
    if (!app || QCoreApplication::closingDown()) return 0;
    return QMetaObject::invokeMethod(app,[] {
        static QPointer<AnchorObserver> installed;
        if (installed) {installed->finish();installed=nullptr;return;}
        HMODULE module=nullptr;
        GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS|GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
            reinterpret_cast<LPCWSTR>(&UnboundStart),&module);
        wchar_t path[32768]; const auto length=GetModuleFileNameW(module,path,32768);
        outputDirectory=QString::fromWCharArray(path,int(length)).section('\\',0,-2);
        installed=new AnchorObserver(qApp);
    },Qt::QueuedConnection)?1:0;
}
#include "anchorprobe.moc"
