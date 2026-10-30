// SPDX-License-Identifier: MIT
#pragma once
#include <QString>
#ifdef UNBOUND_LAYOUT_TRANSITIONS
#include <QApplication>
#include <QMap>
#include <QOpenGLWidget>
#include <QPointer>
#include <QSet>
#include <QTimer>
#include <functional>
namespace Unbound {
// Retain queued work across readiness generations, without idle polling.
class LayoutPendingWork {
public:
    QPointer<QWidget> scope;
    bool pending=false;
    static QList<LayoutPendingWork *> &registry() {static QList<LayoutPendingWork *> value;return value;}
    static void (*&notify())() {static void (*value)()=nullptr;return value;}
    LayoutPendingWork() {registry().append(this);}
    LayoutPendingWork(const LayoutPendingWork &)=delete;
    LayoutPendingWork &operator=(const LayoutPendingWork &)=delete;
    ~LayoutPendingWork() {registry().removeOne(this);if(pending && notify()) notify()();}
    void set(QWidget *widget,bool value=true) {
        if(scope==widget && pending==value)return;
        scope=widget;pending=value;if(notify())notify()();
    }
    void clear() {if(pending) {pending=false;if(notify())notify()();}}
};
// Bounded evidence for presentation, not a guarantee of native renderer completion.
class LayoutReadiness final : public QObject {
public:
    ~LayoutReadiness() override {if(active()==this)active()=nullptr;}
    static LayoutReadiness *&active() {static LayoutReadiness *value=nullptr;return value;}
    static void begin(QWidget *main,const QString &page) {
        if(active())active()->finish();
        new LayoutReadiness(main,page);
    }
    static std::function<void()> observe(std::function<void()> ready,std::function<void()> invalidated) {
        if(auto observer=active()) {
            observer->presentationReady=std::move(ready);observer->presentationInvalidated=std::move(invalidated);
            const QPointer<LayoutReadiness> alive=observer;
            return [alive] {if(alive && active()==alive)alive->finish();};
        }
        return {};
    }
    static void pending(int discovery,int initialization,int toggles,int floating) {
        if(auto observer=active()) {
            observer->m_pending=discovery+initialization+toggles+floating;
            if(observer->m_pending)observer->invalidateCandidate();
            if(observer->m_pending==0 && !observer->finalSurfaces) {observer->surfaces();observer->finalSurfaces=true;}
            observer->quiet.start();
        }
    }
    void finish() {
        if(active()!=this)return;
        active()=nullptr;
        qApp->removeEventFilter(this);deadline.stop();quiet.stop();
        for(const auto &connection:connections)disconnect(connection);
        deleteLater();
    }
private:
    QPointer<QWidget> main;
    QString page;
    QTimer deadline,quiet;
    QSet<QOpenGLWidget *> watched;
    QList<QMetaObject::Connection> connections;
    struct WindowEvidence {
        QPointer<QWidget> window;
        int revision=0,composedRevision=-1,rasterRevision=-1;
        bool hasGl=false;
    };
    QMap<QWidget *,WindowEvidence> windows;
    QMap<QOpenGLWidget *,QPair<QPointer<QWidget>,int>> composing;
    int epoch=0,m_pending=-1,workPending=0,surfaceCount=0;
    bool candidate=false,finalSurfaces=false,finalUpdate=false,presentationReported=false;
    int finalUpdateRounds=0;
    std::function<void()> presentationReady,presentationInvalidated;
    void invalidateCandidate() {
        if(!candidate)return;
        candidate=false;finalUpdate=false;presentationReported=false;
        if(presentationInvalidated)presentationInvalidated();
    }
    void requestFinalUpdate() {
        if(!presentationReady || !candidate || finalUpdate)return;
        if(++finalUpdateRounds>3)return;
        finalUpdate=true;presentationReported=false;
        for(auto &e:windows)if(e.window && e.window->isVisible() && !e.window->isMinimized()) {
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
        int count=0;
        for(auto work:LayoutPendingWork::registry())if(work->pending && relevant(work->scope)) {
            bool otherPage=false;
            for(auto p=work->scope.data();p;p=p->parentWidget())if(p->objectName().startsWith("WStackPage_")) {
                otherPage=p->objectName()!=page;break;
            }
            if(!otherPage)++count;
        }
        workPending=count;if(count)invalidateCandidate();quiet.start();
    }
    void compositionEvidence(bool snapshot=false) {
        if(!candidate)return;
        if(snapshot) {
            for(auto &e:windows)e.hasGl=false;
            for(auto gl:watched)if(gl->isVisible() && relevant(gl))windowEvidence(gl->window()).hasGl=true;
        }
        int waiting=0,visible=0;
        for(const auto &e:windows) {
            if(!e.window || !e.window->isVisible() || e.window->isMinimized())continue;
            ++visible;
            if(e.revision!=(e.hasGl?e.composedRevision:e.rasterRevision))++waiting;
        }
        if(finalUpdate && visible && !waiting && !presentationReported) {
            presentationReported=true;if(presentationReady)presentationReady();
        }
    }
    explicit LayoutReadiness(QWidget *root,const QString &pageName):QObject(qApp),main(root),page(pageName) {
        active()=this;
        deadline.setSingleShot(true);quiet.setSingleShot(true);quiet.setInterval(50);
        connect(&deadline,&QTimer::timeout,this,[this] {finish();});
        connect(qApp,&QCoreApplication::aboutToQuit,this,[this] {finish();});
        connect(&quiet,&QTimer::timeout,this,[this] {
            if(m_pending==0 && !workPending) {candidate=true;compositionEvidence(true);requestFinalUpdate();}
        });
        qApp->installEventFilter(this);surfaces();
        LayoutPendingWork::notify()=[] {if(auto observer=active())observer->workChanged();};
        workChanged();
        // Fallback for an unavailable presentation; normal completion stops sooner.
        deadline.start(3000);
    }
    bool relevant(QWidget *widget) const {
        if(!widget || !main)return false;
        auto window=widget->window();
        return window==main || (window->property("unboundFloatingWindow").toBool()
            && window->property("unboundPage").toString()==page);
    }
    void surfaces() {
        for(auto window:QApplication::topLevelWidgets())if(relevant(window)) {
            windowEvidence(window);
            auto widgets=window->findChildren<QOpenGLWidget *>();
            if(auto gl=qobject_cast<QOpenGLWidget *>(window))widgets.prepend(gl);
            for(auto gl:widgets)if(!watched.contains(gl) && surfaceCount<512) {
                ++surfaceCount;watched.insert(gl);
                connections.append(connect(gl,&QOpenGLWidget::aboutToCompose,this,[this,gl] {
                    auto w=gl->window();if(!relevant(w))return;
                    auto &e=windowEvidence(w);e.hasGl=true;composing[gl]={w,e.revision};
                }));
                connections.append(connect(gl,&QOpenGLWidget::frameSwapped,this,[this,gl] {
                    const auto began=composing.take(gl);auto w=gl->window();
                    if(began.first && began.first==w && relevant(w)) {
                        auto &e=windowEvidence(w);if(e.revision==began.second)e.composedRevision=e.revision;
                        compositionEvidence();
                    }
                }));
                connections.append(connect(gl,&QObject::destroyed,this,[this,gl] {watched.remove(gl);composing.remove(gl);}));
            }
        }
    }
    bool eventFilter(QObject *object,QEvent *event) override {
        const auto type=event->type();
        if(type!=QEvent::UpdateRequest && type!=QEvent::LayoutRequest && type!=QEvent::Move
            && type!=QEvent::Resize && type!=QEvent::Show && type!=QEvent::Hide && type!=QEvent::ParentChange)return false;
        auto w=qobject_cast<QWidget *>(object);if(!relevant(w))return false;
        if(type==QEvent::UpdateRequest) {
            if(finalUpdate && w==w->window()) {
                const QPointer<QWidget> target=w;const int generation=epoch,revision=windowEvidence(w).revision;
                QTimer::singleShot(0,this,[this,target,generation,revision] {
                    if(active()!=this || !target || !candidate || !finalUpdate || epoch!=generation)return;
                    auto &e=windowEvidence(target);
                    if(e.revision==revision) {e.rasterRevision=revision;compositionEvidence();}
                });
            }
        } else {
            invalidateCandidate();++epoch;
            // Layout requests and hidden moves do not prove a changed visible image.
            if(type!=QEvent::LayoutRequest && (w->isVisible() || type==QEvent::Hide))++windowEvidence(w->window()).revision;
            quiet.start();
        }
        return false;
    }
};
}
#else
namespace Unbound {
struct LayoutPendingWork {void set(void *,bool=true) {} void clear() {}};
struct LayoutReadiness {static void begin(void *,const QString &) {} static void pending(int,int,int,int) {}};
}
#endif
