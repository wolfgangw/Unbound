// SPDX-License-Identifier: MIT
#include <QApplication>
#include <QBackingStore>
#include <QDateTime>
#include <QDir>
#include <QElapsedTimer>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QPointer>
#include <QSaveFile>
#include <QSet>
#include <QTimer>
#include <QWidget>
#include <qt_windows.h>
#include <psapi.h>
#include <dxgi1_4.h>

// Diagnostic only. Release exclusively hidden, childless Unbound cache shells;
// their production owners use QPointer and recreate them on the next undock.
// Never release a host containing native content, even if that host is hidden.
class ShellMemory final : public QObject {
    QElapsedTimer clock;
    QJsonArray samples,released;
    bool releaseRequested=false;
    bool selective=false;
    QSet<QString> selected;
    static bool emptyShell(QWidget *w) {
        return w->property("unboundFloatingWindow").toBool()
            && w->objectName().startsWith("unboundFloating_") && w->isHidden()
            && !w->layout() && w->findChildren<QWidget *>().isEmpty();
    }
    void sample() {
        QJsonObject row{{"ms",double(clock.elapsed())}};
        PROCESS_MEMORY_COUNTERS_EX memory{}; memory.cb=sizeof(memory);
        if(GetProcessMemoryInfo(GetCurrentProcess(),reinterpret_cast<PROCESS_MEMORY_COUNTERS *>(&memory),sizeof(memory))) {
            row.insert("private_bytes",double(memory.PrivateUsage));
            row.insert("working_set_bytes",double(memory.WorkingSetSize));
        }
        QJsonArray adapters;
        IDXGIFactory4 *factory=nullptr;
        if(SUCCEEDED(CreateDXGIFactory1(__uuidof(IDXGIFactory4),reinterpret_cast<void **>(&factory)))) {
            IDXGIAdapter1 *adapter=nullptr;
            for(UINT i=0;factory->EnumAdapters1(i,&adapter)!=DXGI_ERROR_NOT_FOUND;++i) {
                DXGI_ADAPTER_DESC1 desc{}; adapter->GetDesc1(&desc);
                IDXGIAdapter3 *modern=nullptr;
                if(SUCCEEDED(adapter->QueryInterface(__uuidof(IDXGIAdapter3),reinterpret_cast<void **>(&modern)))) {
                    QJsonObject gpu{{"adapter",QString::fromWCharArray(desc.Description)}};
                    for(auto group:{DXGI_MEMORY_SEGMENT_GROUP_LOCAL,DXGI_MEMORY_SEGMENT_GROUP_NON_LOCAL}) {
                        DXGI_QUERY_VIDEO_MEMORY_INFO info{};
                        const auto result=modern->QueryVideoMemoryInfo(0,group,&info);
                        const auto key=group==DXGI_MEMORY_SEGMENT_GROUP_LOCAL?"local":"nonlocal";
                        if(SUCCEEDED(result)) gpu.insert(key,QJsonObject{{"current_usage_bytes",double(info.CurrentUsage)},{"budget_bytes",double(info.Budget)}});
                    }
                    adapters.append(gpu); modern->Release();
                }
                adapter->Release();
            }
            factory->Release();
        }
        row.insert("gpu",adapters);
        QJsonArray hosts;
        for(auto w:QApplication::topLevelWidgets()) if(w->property("unboundFloatingWindow").toBool()) {
            auto backing=w->backingStore(); const auto size=backing?backing->size():QSize();
            hosts.append(QJsonObject{{"name",w->objectName()},{"empty_cached",emptyShell(w)},
                {"visible",w->isVisible()},{"widget_children",w->findChildren<QWidget *>().size()},
                {"width",w->width()},{"height",w->height()},{"dpr",w->devicePixelRatioF()},
                {"backing_width",size.width()},{"backing_height",size.height()}});
        }
        row.insert("hosts",hosts); samples.append(row);
    }
    void release() {
        for(auto w:QApplication::topLevelWidgets()) if(emptyShell(w) && (!selective || selected.contains(w->objectName()))) {
            released.append(w->objectName());
#ifdef UNBOUND_SHELL_TRIM
            w->setMinimumSize(0,0); w->resize(1,1);
            if(auto backing=w->backingStore()) backing->resize(w->size());
#else
            delete w;
#endif
        }
    }
    void finish() {
        const auto path=QDir::tempPath()+"/Unbound-shell-memory"; QDir().mkpath(path);
        QSaveFile file(path+"/"+QDateTime::currentDateTimeUtc().toString("yyyyMMdd-HHmmss-zzz")+".json");
        const auto bytes=QJsonDocument(QJsonObject{{"pid",int(GetCurrentProcessId())},
#ifdef UNBOUND_SHELL_TRIM
            {"operation","trim"},
#else
            {"operation","delete"},
#endif
            {"release_requested",releaseRequested},{"release_at_ms",2500},{"released",released},{"samples",samples}}).toJson();
        if(file.open(QIODevice::WriteOnly)) {if(file.write(bytes)==bytes.size()) file.commit();else file.cancelWriting();}
        deleteLater();
    }
public:
    ShellMemory():QObject(qApp) {
        setObjectName("UnboundShellMemoryProbe"); clock.start();
        QFile request(QDir::tempPath()+"/Unbound-shell-memory-request.json");
        if(request.open(QIODevice::ReadOnly)) {
            const auto data=QJsonDocument::fromJson(request.readAll()).object(); request.close();
            releaseRequested=data.value("pid").toInt()==int(GetCurrentProcessId()) && data.value("release").toBool();
            selective=data.contains("hosts");
            for(auto name:data.value("hosts").toArray()) if(name.isString()) selected.insert(name.toString());
            request.remove();
        }
        sample();
        for(int delay:{1000,2000,3500,5000,8000}) QTimer::singleShot(delay,this,[this] {sample();});
        if(releaseRequested) QTimer::singleShot(2500,this,[this] {release();});
        QTimer::singleShot(8500,this,[this] {finish();});
    }
};
extern "C" __declspec(dllexport) DWORD WINAPI UnboundStart(void *) {
    if(!qApp) return 0;
    return QMetaObject::invokeMethod(qApp,[] {
        if(!qApp->findChild<QObject *>("UnboundShellMemoryProbe")) new ShellMemory;
    },Qt::QueuedConnection)?1:0;
}
