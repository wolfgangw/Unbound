// SPDX-License-Identifier: MIT
#pragma once
#include <QString>
#ifdef UNBOUND_LAYOUT_TRACE
#include <QApplication>
#include <QAbstractEventDispatcher>
#include <QDateTime>
#include <QDir>
#include <QElapsedTimer>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QOpenGLWidget>
#include <QPointer>
#include <QSaveFile>
#include <QSet>
#include <QStringList>
#include <QTimer>
#include <QWidget>
#include <array>
#include <functional>
namespace Unbound {
// Diagnostic-only work registry. Persistent members let a new trace see work
// already queued before recall; destruction and cancellation remove it. No timer
// or polling is added to the production lifecycle.
class LayoutTraceWork {
public:
    QString kind;
    QPointer<QWidget> scope;
    bool pending=false;
    static QList<LayoutTraceWork *> &registry() {static QList<LayoutTraceWork *> value;return value;}
    static void (*&notify())() {static void (*value)()=nullptr;return value;}
    LayoutTraceWork() {registry().append(this);}
    LayoutTraceWork(const LayoutTraceWork &)=delete;
    LayoutTraceWork &operator=(const LayoutTraceWork &)=delete;
    ~LayoutTraceWork() {registry().removeOne(this);if(pending && notify()) notify()();}
    void set(const QString &name,QWidget *widget,bool value=true) {
        if(kind==name && scope==widget && pending==value) return;
        kind=name;scope=widget;pending=value;if(notify()) notify()();
    }
    void clear() {if(pending) {pending=false;if(notify()) notify()();}}
};
// Internal-only, GUI-thread observation. Buffer during recall; write once after
// a bounded observation window. Paint traffic never delays the quiet candidate.
class LayoutTrace final : public QObject {
public:
    ~LayoutTrace() override { if(active()==this) active()=nullptr; }
    static LayoutTrace *&active() { static LayoutTrace *value=nullptr; return value; }
    static void begin(QWidget *main,const QString &page) {
        if(active()) active()->finish("superseded");
        new LayoutTrace(main,page);
    }
    static void mark(const QString &phase,const QString &pane={},qint64 duration=-1) {
        if(auto trace=active()) trace->record(phase,pane,duration);
    }
    static void presentationProbe(std::function<void()> ready,std::function<void()> invalidated) {
        if(auto trace=active()) {trace->presentationReady=std::move(ready);trace->presentationInvalidated=std::move(invalidated);}
    }
    static void pending(int discovery,int initialization,int toggles,int floating) {
        if(auto trace=active()) {
            trace->m_pending=discovery+initialization+toggles+floating;
            if(trace->m_pending)trace->invalidateCandidate("candidate_invalidated_by_controller");
            trace->append(QJsonObject{{"us",double(trace->clock.nsecsElapsed()/1000)},
                {"phase","controller_pending"},{"discovery",discovery},{"initialization",initialization},
                {"toggles",toggles},{"floating",floating}});
            if(trace->m_pending==0 && !trace->finalSurfaces) {trace->surfaces();trace->finalSurfaces=true;}
            trace->quiet.start();
        }
    }
private:
    QPointer<QWidget> main;
    QString page;
    QElapsedTimer clock;
    QTimer deadline,quiet;
    QJsonArray rows;
    // Keep hot paths numeric. Structural traffic must never exhaust the phase
    // buffer, and constructing JSON for every paint distorts the measured work.
    struct Samples {
        qint64 count=0,first=0,last=0,maxGap=0,maxGapEnd=0,totalDuration=0,maxDuration=0;
        std::array<int,60> bins{};
        void add(qint64 now,qint64 duration=-1) {
            if(!count) first=now;
            else if(now-last>maxGap) {maxGap=now-last;maxGapEnd=now;}
            last=now; ++count;
            ++bins[size_t(qBound<qint64>(0,now/100000,59))];
            if(duration>=0) {totalDuration+=duration;maxDuration=qMax(maxDuration,duration);}
        }
        QJsonObject json() const {
            QJsonArray histogram; for(auto n:bins) histogram.append(n);
            return {{"count",double(count)},{"first_us",double(first)},{"last_us",double(last)},
                {"max_gap_us",double(maxGap)},{"max_gap_end_us",double(maxGapEnd)},
                {"total_duration_us",double(totalDuration)},{"max_duration_us",double(maxDuration)},
                {"counts_per_100ms",histogram}};
        }
    };
    QMap<QString,Samples> paints,structures,phases;
    Samples idle;
    QJsonArray surfaceInfo;
    QSet<QObject *> watched;
    QList<QMetaObject::Connection> connections;
    struct WindowEvidence {
        QPointer<QWidget> window;
        int revision=0,composedRevision=-1,rasterRevision=-1;
        bool hasGl=false;
    };
    QMap<QWidget *,WindowEvidence> windows;
    QMap<QOpenGLWidget *,QPair<QPointer<QWidget>,int>> composing;
    int epoch=0,m_pending=-1,dropped=0,surfaceSequence=0,workPending=0;
    bool candidate=false,finalSurfaces=false,glReported=false;
    bool finalUpdate=false,presentationReported=false;
    int finalUpdateRounds=0;
    std::function<void()> presentationReady,presentationInvalidated;
    void invalidateCandidate(const QString &reason) {
        if(!candidate)return;
        record(reason);candidate=false;glReported=false;finalUpdate=false;presentationReported=false;
        if(presentationInvalidated)presentationInvalidated();
    }
    void requestFinalUpdate() {
        if(!presentationReady || !candidate || finalUpdate)return;
        // Bounded experiment: one coalesced update per stable candidate, never a paint loop.
        if(++finalUpdateRounds>3) {record("final_update_attempt_limit");return;}
        finalUpdate=true;presentationReported=false;
        record("final_update_requested");
        for(auto &e:windows) if(e.window && e.window->isVisible() && !e.window->isMinimized()) {
            e.composedRevision=-1;e.rasterRevision=-1;e.window->update();
        }
    }
    WindowEvidence &windowEvidence(QWidget *window) {
        if(!windows.contains(window)) {
            windows[window].window=window;
            connections.append(connect(window,&QObject::destroyed,this,[this,window] {windows.remove(window);}));
        }
        return windows[window];
    }
    void workChanged() {
        int count=0; QJsonArray names;
        for(auto work:LayoutTraceWork::registry()) if(work->pending && relevant(work->scope)) {
            bool otherPage=false;
            for(auto p=work->scope.data();p;p=p->parentWidget()) if(p->objectName().startsWith("WStackPage_")) {
                otherPage=p->objectName()!=page;break;
            }
            if(otherPage) continue;
            ++count; names.append(work->kind+":"+work->scope->objectName());
        }
        workPending=count;
        append(QJsonObject{{"us",double(clock.nsecsElapsed()/1000)},{"phase","adapter_pending"},
            {"count",count},{"work",names}});
        if(count)invalidateCandidate("candidate_invalidated_by_work");
        quiet.start();
    }
    void compositionEvidence(bool snapshot=false) {
        if(!candidate) return;
        if(snapshot) {
            for(auto &e:windows) e.hasGl=false;
            for(auto object:watched) if(auto gl=qobject_cast<QOpenGLWidget *>(object))
                if(gl->isVisible() && relevant(gl)) windowEvidence(gl->window()).hasGl=true;
        }
        int gl=0,waiting=0,raster=0,rasterWaiting=0,visible=0; QJsonArray states;
        for(auto it=windows.cbegin();it!=windows.cend();++it) {
            const auto &e=it.value(); auto w=e.window.data();
            if(!w || !w->isVisible() || w->isMinimized()) continue;
            ++visible;
            if(e.hasGl) {++gl;if(e.revision!=e.composedRevision)++waiting;} else ++raster;
            if(!e.hasGl && e.revision!=e.rasterRevision)++rasterWaiting;
            if(snapshot) states.append(QJsonObject{{"window",w->objectName()},{"geometry_revision",e.revision},
                {"composed_revision",e.composedRevision},{"has_gl",e.hasGl}});
        }
        if(snapshot) append(QJsonObject{{"us",double(clock.nsecsElapsed()/1000)},
            {"phase","composition_evidence"},{"windows",states},{"gl_waiting",waiting},{"raster_unproven",raster}});
        if(gl && !waiting && !glReported) {
            glReported=true;record("all_visible_gl_windows_composed_not_global_ready");
        }
        if(finalUpdate && visible && !waiting && !rasterWaiting && !presentationReported) {
            presentationReported=true;record("post_update_candidate_not_native_ready");
            if(presentationReady)presentationReady();
        }
    }
    explicit LayoutTrace(QWidget *root,const QString &pageName):QObject(qApp),main(root),page(pageName) {
        active()=this; clock.start();
        deadline.setSingleShot(true); quiet.setSingleShot(true); quiet.setInterval(50);
        connect(&deadline,&QTimer::timeout,this,[this] {finish("deadline");});
        connect(qApp,&QCoreApplication::aboutToQuit,this,[this] {if(active()==this) finish("shutdown");});
        connect(&quiet,&QTimer::timeout,this,[this] {
            if(m_pending==0) {
                record("quiet_candidate_not_gpu_ready");
                if(!workPending) {candidate=true;record("work_and_geometry_quiet");compositionEvidence(true);requestFinalUpdate();}
                else record("quiet_blocked_by_adapter_work");
            }
        });
        if(auto dispatcher=QAbstractEventDispatcher::instance())
            connections.append(connect(dispatcher,&QAbstractEventDispatcher::aboutToBlock,this,[this] {
                idle.add(clock.nsecsElapsed()/1000);
            }));
        record("recall_begin"); qApp->installEventFilter(this); surfaces();
        LayoutTraceWork::notify()=[] {if(auto trace=active())trace->workChanged();};
        workChanged();
        const int generation=epoch;
        QTimer::singleShot(0,this,[this,generation] {
            record("queued_marker");
            if(generation!=epoch) record("marker_geometry_changed");
        });
        deadline.start(6000);
    }
    bool relevant(QWidget *w) const {
        if(!w || !main) return false;
        auto window=w->window();
        return window==main || (window->property("unboundFloatingWindow").toBool()
            && window->property("unboundPage").toString()==page);
    }
    void record(const QString &phase,const QString &pane={},qint64 duration=-1) {
        const auto now=clock.nsecsElapsed()/1000;
        if(phases.size()<256 || phases.contains(phase)) phases[phase].add(now,duration);
        QJsonObject row{{"us",double(now)},{"phase",phase},{"epoch",epoch}};
        if(!pane.isEmpty()) row.insert("pane",pane);
        if(duration>=0) row.insert("duration_us",double(duration));
        append(row);
    }
    void append(const QJsonObject &row) {
        if(rows.size()>=6000) {++dropped;return;}
        rows.append(row);
    }
    void paint(const QString &name,const QString &kind) {
        const auto key=name+":"+kind;
        if(paints.size()<1024 || paints.contains(key)) paints[key].add(clock.nsecsElapsed()/1000);
    }
    void surfaces() {
        for(auto window:QApplication::topLevelWidgets()) if(relevant(window)) {
            windowEvidence(window);
            auto widgets=window->findChildren<QOpenGLWidget *>();
            if(auto gl=qobject_cast<QOpenGLWidget *>(window)) widgets.prepend(gl);
            for(auto gl:widgets) if(!watched.contains(gl) && surfaceSequence<512) {
                watched.insert(gl);
                // Stable local identity survives reparenting and disambiguates
                // repeated/empty object names. No pointer addresses in output.
                const auto name=QString("gl%1/%2").arg(++surfaceSequence).arg(gl->objectName());
                QStringList ancestry;
                for(auto p=gl->parentWidget();p;p=p->parentWidget()) ancestry.prepend(p->objectName());
                surfaceInfo.append(QJsonObject{{"id",name},{"class",gl->metaObject()->className()},
                    {"initial_window",window->objectName()},{"ancestry",ancestry.join('/')},
                    {"visible_at_binding",gl->isVisible()}});
                connections.append(connect(gl,&QOpenGLWidget::aboutToCompose,this,[this,gl] {
                    auto w=gl->window();if(!relevant(w))return;
                    auto &e=windowEvidence(w);e.hasGl=true;composing[gl]={w,e.revision};
                }));
                connections.append(connect(gl,&QOpenGLWidget::frameSwapped,this,[this,name,gl] {
                    paint(name,"frameSwapped");
                    const auto began=composing.take(gl);auto w=gl->window();
                    if(began.first && began.first==w && relevant(w)) {
                        auto &e=windowEvidence(w);
                        if(e.revision==began.second) e.composedRevision=e.revision;
                        compositionEvidence();
                    }
                }));
                connections.append(connect(gl,&QObject::destroyed,this,[this,gl] {watched.remove(gl);composing.remove(gl);}));
            }
        }
    }
    bool eventFilter(QObject *object,QEvent *event) override {
        const auto type=event->type();
        if(type!=QEvent::Paint && type!=QEvent::UpdateRequest && type!=QEvent::LayoutRequest
            && type!=QEvent::Move && type!=QEvent::Resize && type!=QEvent::Show
            && type!=QEvent::Hide && type!=QEvent::ParentChange) return false;
        auto w=qobject_cast<QWidget *>(object);
        if(!relevant(w)) return false;
        if(type==QEvent::Paint || type==QEvent::UpdateRequest) {
            paint(w->window()->objectName(),type==QEvent::Paint?"paint_delivery_not_completion":"update_request");
            if(type==QEvent::UpdateRequest && finalUpdate && w==w->window()) {
                const QPointer<QWidget> target=w;const int generation=epoch,revision=windowEvidence(w).revision;
                QTimer::singleShot(0,this,[this,target,generation,revision] {
                    if(!target || !candidate || !finalUpdate || epoch!=generation)return;
                    auto &e=windowEvidence(target);
                    if(e.revision==revision) {e.rasterRevision=revision;compositionEvidence();}
                });
            }
        } else {
            invalidateCandidate("candidate_invalidated");
            ++epoch;
            // LayoutRequest is potential work, not proof of a changed allocation.
            // Hidden descendant moves cannot invalidate an already composed visible image.
            if(type!=QEvent::LayoutRequest && (w->isVisible() || type==QEvent::Hide))
                ++windowEvidence(w->window()).revision;
            const auto key=w->window()->objectName()+":"+QString::number(int(type));
            if(structures.size()<256 || structures.contains(key)) structures[key].add(clock.nsecsElapsed()/1000);
            quiet.start();
        }
        return false;
    }
    void finish(const QString &reason) {
        record(reason); qApp->removeEventFilter(this);
        deadline.stop(); quiet.stop();
        for(const auto &connection:connections) disconnect(connection);
        auto summaries=[](const QMap<QString,Samples> &samples) {
            QJsonObject result; for(auto it=samples.cbegin();it!=samples.cend();++it) result.insert(it.key(),it.value().json());
            return result;
        };
        const auto overridePath=qEnvironmentVariable("UNBOUND_LAYOUT_TRACE_DIR");
        const auto path=overridePath.isEmpty()?QDir::tempPath()+"/Unbound-layout-traces":overridePath; QDir().mkpath(path);
        static unsigned sequence=0;
        QSaveFile file(path+"/"+QDateTime::currentDateTimeUtc().toString("yyyyMMdd-HHmmss-zzz")+"-"+QString::number(++sequence)+".json");
        const auto bytes=QJsonDocument(QJsonObject{{"schema",4},{"qt",qVersion()},{"page",page},{"events",rows},
            {"paint_summary",summaries(paints)},{"structure_summary",summaries(structures)},
            {"phase_summary",summaries(phases)},{"idle_summary",idle.json()},{"gl_surfaces",surfaceInfo},
            {"finish_reason",reason},{"elapsed_us",double(clock.nsecsElapsed()/1000)},{"structural_events",epoch},
            {"dropped_events",dropped},{"pending_at_end",m_pending},{"adapter_pending_at_end",workPending}}).toJson();
        if(file.open(QIODevice::WriteOnly)) {if(file.write(bytes)==bytes.size()) file.commit();else file.cancelWriting();}
        active()=nullptr; deleteLater();
    }
};
class LayoutTraceSpan {
    QString phase,pane; QElapsedTimer clock; LayoutTrace *trace;
public:
    LayoutTraceSpan(QString name,QString target={}):phase(std::move(name)),pane(std::move(target)),trace(LayoutTrace::active()) {if(trace)clock.start();}
    ~LayoutTraceSpan() {if(trace && trace==LayoutTrace::active()) LayoutTrace::mark(phase,pane,clock.nsecsElapsed()/1000);}
    void next(const QString &name) {
        if(trace && trace==LayoutTrace::active()) {LayoutTrace::mark(phase,pane,clock.nsecsElapsed()/1000);clock.restart();}
        phase=name;
    }
};
}
#else
namespace Unbound {
struct LayoutTraceWork {void set(const QString &,void *,bool=true) {} void clear() {}};
struct LayoutTrace {
    static void begin(void *,const QString &) {}
    static void mark(const QString &,const QString & = {},long long=-1) {}
    static void pending(int,int,int,int) {}
};
struct LayoutTraceSpan { LayoutTraceSpan(const QString &,const QString & = {}) {} void next(const QString &) {} };
}
#endif
