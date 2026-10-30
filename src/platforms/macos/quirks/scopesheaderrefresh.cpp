// SPDX-License-Identifier: MIT
#include "widgetquirks.h"
#include <QEvent>
#include <QPointer>
#include <QTimer>
#include <QWidget>
namespace Unbound {
namespace {
// On Cocoa, retained Scopes can return with visible but unpainted headers.
// Resolve can initially show this child at its former top-level position,
// outside the host. The later layout Move must invalidate headers again; a
// show-only update is clipped away before the child returns to the host.
// Invalidate only native header subtrees after queued show/move layout work.
// No focus, visibility, geometry or native renderer changes are needed.
class ScopesHeaderRefresh final : public QObject {
public:
    ScopesHeaderRefresh(QWidget *scopes,QWidget *host):m_scopes(scopes),m_host(host) {
        scopes->installEventFilter(this);
        if(scopes->isVisible())request();
    }
protected:
    bool eventFilter(QObject *,QEvent *event) override {
        if(event->type()==QEvent::Show || event->type()==QEvent::Move)request();
        else if(event->type()==QEvent::Hide || event->type()==QEvent::ParentChange)++m_generation;
        return false;
    }
private:
    void request() {
        const auto generation=++m_generation;
        QTimer::singleShot(0,this,[this,generation]{
            if(generation!=m_generation)return;
            QTimer::singleShot(0,this,[this,generation]{
                if(generation!=m_generation || !m_scopes || !m_host
                    || m_scopes->parentWidget()!=m_host || !m_scopes->isVisible())return;
                for(auto header:m_scopes->findChildren<QWidget *>())
                    if(header->isVisible() && (header->objectName()==QLatin1String("titleFrame")
                        || header->objectName()==QLatin1String("headerFrame")))header->update();
            });
        });
    }
    QPointer<QWidget> m_scopes,m_host;
    unsigned m_generation=0;
};
}
std::unique_ptr<QObject> attachScopesWidgetQuirks(QWidget *scopes,QWidget *host) {
    return std::make_unique<ScopesHeaderRefresh>(scopes,host);
}
}
