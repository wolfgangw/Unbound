#pragma once
#include "panefloatsession.h"
#include <QApplication>
#include <QCoreApplication>
#include <QDebug>
#include <QLibrary>
#include <QTemporaryDir>
#include <QStackedWidget>
#include "panesettings.h"
#include "mainwindowplacement.h"
#include "sidebarfloatsession.h"
#include <QPushButton>
#include <QGraphicsRectItem>
#include <QMessageBox>
#include "panedecoration.h"
#include <cstdlib>
#include <cstdio>
#include "platformtests.h"
#include "panesnap.h"

inline void check(bool ok, const char *message)
{
    if (!ok) { std::fprintf(stderr, "FAIL: %s\n", message); std::exit(1); }
}

// Qt 5.15.2's Windows QMessageBox::showEvent accesses a native system menu,
// which the offscreen platform does not provide. Capture expected warnings
// before that handler, while still testing the controller's error path.
class WarningCapture : public QObject
{
public:
    QStringList messages;
    bool eventFilter(QObject *object, QEvent *event) override
    {
        auto box = qobject_cast<QMessageBox *>(object);
        if (!box || event->type() != QEvent::Show) return false;
        messages.append(box->text());
        QTimer::singleShot(0, box, [box] { box->done(QMessageBox::Ok); });
        return true;
    }
};

class HostCloseInterceptor : public QObject
{
public:
    QPointer<QWidget> host;
    int closeEvents = 0;
    bool eventFilter(QObject *object, QEvent *event) override
    {
        if (!host || object != host || event->type() != QEvent::Close) return false;
        ++closeEvents;
        host->hide();
        event->accept();
        return true;
    }
};

class ResizeObservedGraph : public QGraphicsView
{
public:
    using QGraphicsView::QGraphicsView;
    int resizeCalls=0;
    QRectF sceneAtResize;
protected:
    void resizeEvent(QResizeEvent *event) override
    {
        ++resizeCalls;
        sceneAtResize=scene() ? scene()->sceneRect() : QRectF();
        QGraphicsView::resizeEvent(event);
    }
};

// Models the native UiElidedLabel's observed full-text minimum-size hint.
class ElidingTitleLabel : public QLabel
{
public:
    using QLabel::QLabel;
    QSize minimumSizeHint() const override { return sizeHint(); }
};

