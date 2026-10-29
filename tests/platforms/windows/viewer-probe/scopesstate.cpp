// SPDX-License-Identifier: MIT
#include <QApplication>
#include <QAbstractButton>
#include <QMetaMethod>
#include <QFile>
#include <QDir>
#include <QJsonDocument>
#include <QJsonArray>
#include <QJsonObject>
#include <QSet>
#include <qt_windows.h>
extern "C" __declspec(dllexport) DWORD WINAPI UnboundStart(void *) {
 return QMetaObject::invokeMethod(qApp,[]{
 QSet<QWidget *> included;QJsonArray rows;
 for(auto w:QApplication::allWidgets())if(w->objectName().contains("scope",Qt::CaseInsensitive) || w->objectName().contains("waveform",Qt::CaseInsensitive)) {
  included.insert(w);for(auto c:w->findChildren<QWidget *>())included.insert(c);
  for(auto p=w->parentWidget();p;p=p->parentWidget())included.insert(p);
 }
 for(auto w:included) {
  QJsonArray methods;
  for(int i=QWidget::staticMetaObject.methodCount();i<w->metaObject()->methodCount();++i)methods.append(QString::fromLatin1(w->metaObject()->method(i).methodSignature()));
  QJsonObject row{{"id",QString::number(quintptr(w),16)},{"parent",QString::number(quintptr(w->parentWidget()),16)},
   {"name",w->objectName()},{"class",w->metaObject()->className()},{"window",w->window()->objectName()},
   {"visible",w->isVisible()},{"hidden",w->isHidden()},{"methods",methods},
   {"cursor",int(w->cursor().shape())},{"explicitCursor",w->testAttribute(Qt::WA_SetCursor)},
   {"overrideCursor",QApplication::overrideCursor()?int(QApplication::overrideCursor()->shape()):-1},
   {"geometry",QJsonArray{w->x(),w->y(),w->width(),w->height()}}};
  if(auto b=qobject_cast<QAbstractButton *>(w)){row.insert("tooltip",b->toolTip());row.insert("checked",b->isChecked());row.insert("enabled",b->isEnabled());}
  rows.append(row);
 }
 QFile f(QDir::tempPath()+"/Unbound-scopes-state.json");if(f.open(QIODevice::WriteOnly))f.write(QJsonDocument(rows).toJson());
 },Qt::QueuedConnection)?1:0;
}
