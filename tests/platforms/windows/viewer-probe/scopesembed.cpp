// SPDX-License-Identifier: MIT
#include <QApplication>
#include <QDialog>
#include <QPointer>
#include <QVBoxLayout>
#include <qt_windows.h>
class Embed final : public QObject {
public:
 QPointer<QDialog> dialog;QPointer<QWidget> original,host;Qt::WindowFlags flags;QRect geometry;QSize minimum;
 explicit Embed(QDialog *d):QObject(qApp),dialog(d),original(d->parentWidget()),flags(d->windowFlags()),geometry(d->geometry()),minimum(d->minimumSize()) {
  host=new QWidget(nullptr,Qt::Window);host->setWindowTitle("Scopes experiment");
  host->setAttribute(Qt::WA_ShowWithoutActivating);auto layout=new QVBoxLayout(host);layout->setContentsMargins(0,0,0,0);
  d->setParent(host,Qt::Widget);d->setMinimumSize(0,0);layout->addWidget(d);d->show();host->resize(700,400);host->show();
 }
 ~Embed()override{if(dialog){host->layout()->removeWidget(dialog);dialog->setParent(original,flags);dialog->setMinimumSize(minimum);dialog->setGeometry(geometry);dialog->show();}delete host;}
};
static QPointer<Embed> active;
extern "C" __declspec(dllexport) DWORD WINAPI UnboundStart(void *) {
 return QMetaObject::invokeMethod(qApp,[]{
  if(active){delete active;return;}
  for(auto w:QApplication::topLevelWidgets())if(w->objectName()=="UiWaveformWindow" && w->isVisible())if(auto d=qobject_cast<QDialog *>(w)){active=new Embed(d);break;}
 },Qt::QueuedConnection)?1:0;
}
