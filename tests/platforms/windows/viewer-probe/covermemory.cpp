// SPDX-License-Identifier: MIT
#include "layouttransitionpresentation.h"
#include <QApplication>
#include <QDir>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSaveFile>
#include <QTimer>
#include <qt_windows.h>
#include <psapi.h>
#include <dxgi1_4.h>
class CoverMemory final : public QObject {
    QJsonArray rows;int roundIndex=0;
    std::unique_ptr<Unbound::LayoutTransitionPresentation> cover;
    void sample(const QString &phase) {
        PROCESS_MEMORY_COUNTERS_EX memory{};memory.cb=sizeof(memory);
        GetProcessMemoryInfo(GetCurrentProcess(),reinterpret_cast<PROCESS_MEMORY_COUNTERS *>(&memory),sizeof(memory));
        QJsonObject row{{"round",roundIndex},{"phase",phase},{"private_bytes",double(memory.PrivateUsage)},
            {"working_set_bytes",double(memory.WorkingSetSize)},{"bitmap_bytes",double(cover?cover->retainedBytes():0)}};
        QJsonArray adapters;IDXGIFactory4 *factory=nullptr;
        if(SUCCEEDED(CreateDXGIFactory1(__uuidof(IDXGIFactory4),reinterpret_cast<void **>(&factory)))) {
            IDXGIAdapter1 *adapter=nullptr;
            for(UINT i=0;factory->EnumAdapters1(i,&adapter)!=DXGI_ERROR_NOT_FOUND;++i) {
                DXGI_ADAPTER_DESC1 desc{};adapter->GetDesc1(&desc);IDXGIAdapter3 *modern=nullptr;
                if(SUCCEEDED(adapter->QueryInterface(__uuidof(IDXGIAdapter3),reinterpret_cast<void **>(&modern)))) {
                    QJsonObject gpu{{"adapter",QString::fromWCharArray(desc.Description)},
                        {"luid",QString::number(desc.AdapterLuid.HighPart)+":"+QString::number(desc.AdapterLuid.LowPart)}};
                    for(auto group:{DXGI_MEMORY_SEGMENT_GROUP_LOCAL,DXGI_MEMORY_SEGMENT_GROUP_NON_LOCAL}) {
                        DXGI_QUERY_VIDEO_MEMORY_INFO info{};
                        if(SUCCEEDED(modern->QueryVideoMemoryInfo(0,group,&info)))gpu.insert(group==DXGI_MEMORY_SEGMENT_GROUP_LOCAL?"local_bytes":"nonlocal_bytes",double(info.CurrentUsage));
                    }
                    adapters.append(gpu);modern->Release();
                }
                adapter->Release();
            }
            factory->Release();
        }
        row.insert("gpu",adapters);rows.append(row);
    }
    void startRound() {
        ++roundIndex;cover=Unbound::makeLayoutTransitionPresentation();sample("before");
        if(!cover->begin(QApplication::activeWindow(),[this]{sample("interrupted");finish();})) {
            sample("capture_failed");finish();return;
        }
        sample("captured");
        QTimer::singleShot(300,this,[this]{
            sample("held");cover->cancel();cover.reset();sample("released");
            QTimer::singleShot(700,this,[this]{sample("settled");if(this->roundIndex < 3)startRound();else finish();});
        });
    }
    void finish() {
        if(cover)cover->cancel();
        QSaveFile file(QDir::tempPath()+"/Unbound-cover-memory.json");
        if(file.open(QIODevice::WriteOnly)){file.write(QJsonDocument(rows).toJson());file.commit();}
        deleteLater();
    }
public:
    CoverMemory():QObject(qApp) {setObjectName("UnboundCoverMemory");startRound();}
};
extern "C" __declspec(dllexport) DWORD WINAPI UnboundStart(void *) {
    if(!qApp)return 0;
    return QMetaObject::invokeMethod(qApp,[]{if(!qApp->findChild<QObject *>("UnboundCoverMemory"))new CoverMemory;},Qt::QueuedConnection)?1:0;
}
