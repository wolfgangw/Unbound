// SPDX-License-Identifier: MIT
#pragma once
#include <QWidget>
#ifdef UNBOUND_PALETTE_SIZE_DIAGNOSTICS
#include <QApplication>
#include <QDateTime>
#include <QDir>
#include <QElapsedTimer>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QPointer>
#include <QSaveFile>
#include <QTimer>

namespace Unbound {
// Passive, bounded startup evidence. Outlives the restoration observer so a
// late native minimum/resize remains visible after correction has stopped.
class PaletteRestorationDiagnostics final : public QObject {
public:
    PaletteRestorationDiagnostics(QWidget *host,QSize requested) : QObject(qApp),m_host(host),m_requested(requested) {
        m_name=host->objectName();
        m_stamp=QDateTime::currentDateTimeUtc().toString("yyyyMMdd-HHmmss-zzz");
        m_clock.start();
        record("begin");
        qApp->installEventFilter(this);
        connect(host,&QObject::destroyed,this,[this] { finish(); });
        QTimer::singleShot(10000,this,[this] { record("deadline"); finish(); });
    }
    void record(const QString &phase) {
        if(m_finished || m_rows.size()>=1200) return;
        QJsonObject row{{"ms",double(m_clock.elapsed())},{"phase",phase}};
        if(m_host) {
            const auto geometry=m_host->geometry();
            const auto minimum=m_host->minimumSize(),hint=m_host->minimumSizeHint();
            row.insert("geometry",QJsonArray{geometry.x(),geometry.y(),geometry.width(),geometry.height()});
            row.insert("minimum",QJsonArray{minimum.width(),minimum.height()});
            row.insert("minimumHint",QJsonArray{hint.width(),hint.height()});
            row.insert("visible",m_host->isVisible());
        }
        m_rows.append(row);
    }
protected:
    bool eventFilter(QObject *object,QEvent *event) override {
        if(object!=m_host) return false;
        const auto type=event->type();
        if(type!=QEvent::Resize && type!=QEvent::Move && type!=QEvent::Show
            && type!=QEvent::Hide && type!=QEvent::LayoutRequest && type!=QEvent::WindowStateChange) return false;
        const auto phase=QString::number(int(type));
        record(phase);
        if(!m_queued) {
            m_queued=true;
            QTimer::singleShot(0,this,[this] {m_queued=false;record("queued");});
        }
        return false;
    }
private:
    void finish() {
        if(m_finished) return;
        m_finished=true;
        qApp->removeEventFilter(this);
        const auto path=QDir::tempPath()+"/Unbound-palette-restoration";
        QDir().mkpath(path);
        static unsigned sequence=0;
        QSaveFile file(path+"/"+m_stamp+"-"+QString::number(QCoreApplication::applicationPid())+"-"+QString::number(++sequence)+".json");
        if(file.open(QIODevice::WriteOnly)) {
            const auto bytes=QJsonDocument(QJsonObject{{"host",m_name},
                {"requested",QJsonArray{m_requested.width(),m_requested.height()}},{"events",m_rows}}).toJson();
            if(file.write(bytes)==bytes.size())file.commit();else file.cancelWriting();
        }
        deleteLater();
    }
    QPointer<QWidget> m_host;
    QSize m_requested;
    QString m_name,m_stamp;
    QElapsedTimer m_clock;
    QJsonArray m_rows;
    bool m_finished=false,m_queued=false;
};
}
#endif
