// SPDX-License-Identifier: MIT
#include "layouttracediagnostics.h"
#include "layoutadapter.h"
#ifdef UNBOUND_LAYOUT_TRACE
#include <QFile>
#include <QResizeEvent>
#include <QTemporaryDir>

void testLayoutTrace() {
    using namespace Unbound;
    using LayoutTest::require;
    QTemporaryDir directory;
    require(directory.isValid(),"Cannot create trace fixture output directory");
    const auto previous=qgetenv("UNBOUND_LAYOUT_TRACE_DIR");
    qputenv("UNBOUND_LAYOUT_TRACE_DIR",directory.path().toUtf8());
    struct ObservedWidget : QWidget {
        int resizeEvents=0,updates=0;
        bool event(QEvent *event) override {
            if(event->type()==QEvent::Resize) {++resizeEvents;return true;}
            if(event->type()==QEvent::UpdateRequest) {++updates;return true;}
            return QWidget::event(event);
        }
    } root;
    root.setObjectName("trace_fixture");
    LayoutTrace::begin(&root,"fixture");
    for(int i=0;i<20000;++i) {
        QResizeEvent resize(QSize(300+i%2,300),QSize(300,300));
        QApplication::sendEvent(&root,&resize);
    }
    LayoutTrace::pending(0,0,0,0);
    QTimer paints;
    QObject::connect(&paints,&QTimer::timeout,&root,[&root] {
        QEvent update(QEvent::UpdateRequest); QApplication::sendEvent(&root,&update);
    });
    paints.start(5);
    QEventLoop loop; QTimer::singleShot(100,&loop,&QEventLoop::quit); loop.exec();
    paints.stop();
    QResizeEvent correction(QSize(301,300),QSize(300,300)); QApplication::sendEvent(&root,&correction);
    LayoutTrace::mark("late_milestone",{},123);
    require(root.resizeEvents==20001 && root.updates>0,"Trace consumed layout/paint delivery");
    LayoutTrace::begin(&root,"replacement"); // Flush the measured generation.
    delete LayoutTrace::active();
    require(!LayoutTrace::active(),"Destroyed trace retained a dangling active pointer");
    const auto files=QDir(directory.path()).entryList({"*.json"},QDir::Files);
    require(files.size()==1,"Superseded trace was not flushed exactly once");
    QFile file(directory.filePath(files.first())); require(file.open(QIODevice::ReadOnly),"Cannot read trace output");
    const auto data=QJsonDocument::fromJson(file.readAll()).object();
    require(data.value("dropped_events").toInt()==0 && data.value("structural_events").toInt()==20001,
        "Structural burst exhausted important trace rows or lost aggregate counts");
    require(data.value("finish_reason")=="superseded" && data.value("pending_at_end").toInt()==0,
        "Trace lost its terminal state");
    const auto phases=data.value("phase_summary").toObject();
    require(phases.contains("late_milestone") && phases.contains("quiet_candidate_not_gpu_ready")
        && phases.contains("candidate_invalidated"),"Late milestone or quiet/invalidation evidence missing");
    require(phases.value("late_milestone").toObject().value("total_duration_us").toInt()==123,
        "Trace lost inclusive phase duration");
    LayoutTraceWork work;
    work.set("fixture_pending",&root); // Queued before the new recall starts.
    LayoutTrace::begin(&root,"work_fixture");
    LayoutTrace::pending(0,0,0,0);
    auto wait=[] {QEventLoop loop;QTimer::singleShot(100,&loop,&QEventLoop::quit);loop.exec();};
    wait();work.clear();wait();
    work.set("late_pending",&root);
    LayoutTrace::begin(&root,"replacement");delete LayoutTrace::active();
    const auto workFiles=QDir(directory.path()).entryList({"*.json"},QDir::Files,QDir::Name);
    require(workFiles.size()==2,"Work trace did not flush exactly once");
    QFile workFile(directory.filePath(workFiles.last()));require(workFile.open(QIODevice::ReadOnly),"Cannot read work trace");
    const auto workData=QJsonDocument::fromJson(workFile.readAll()).object();
    const auto workPhases=workData.value("phase_summary").toObject();
    require(workPhases.contains("quiet_blocked_by_adapter_work") && workPhases.contains("work_and_geometry_quiet")
        && workPhases.contains("candidate_invalidated_by_work") && workData.value("adapter_pending_at_end").toInt()==1,
        "Adapter work did not block, release and invalidate the candidate across trace generations");
    require(workPhases.value("work_and_geometry_quiet").toObject().value("first_us").toDouble()>=100000,
        "Work candidate was emitted while earlier queued work remained pending");
    work.clear();
    // Exercise the additional presentation probe with real raster update delivery.
    // Earlier work must block it, and later layout changes must revoke its candidate.
    QWidget presentation;presentation.setObjectName("presentation_fixture");presentation.show();
    LayoutTest::settle();
    int reveals=0,invalidations=0;
    LayoutTraceWork delayed;delayed.set("delayed",&presentation);
    LayoutTrace::begin(&presentation,"presentation");
    LayoutTrace::presentationProbe([&] {++reveals;},[&] {++invalidations;});
    LayoutTrace::pending(0,0,0,0);wait();
    require(!reveals,"Presentation candidate ignored pending adapter work");
    delayed.clear();wait();wait();
    require(reveals==1,"Final raster update did not yield a single presentation candidate");
    presentation.resize(presentation.width()+1,presentation.height());wait();wait();
    require(invalidations>=1 && reveals==2,"Geometry change failed to invalidate and recheck presentation");
    delete LayoutTrace::active();
    if(previous.isNull()) qunsetenv("UNBOUND_LAYOUT_TRACE_DIR"); else qputenv("UNBOUND_LAYOUT_TRACE_DIR",previous);
    std::puts("PASS: recall trace preserves late milestones under structural bursts, ignores paint for quiet detection, and observes without consumption");
}
#else
void testLayoutTrace() {}
#endif
