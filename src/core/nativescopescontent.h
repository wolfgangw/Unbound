// SPDX-License-Identifier: MIT
#pragma once
#include <QAbstractButton>
#include <QDialog>
#include <QKeyEvent>
#include <QPointer>
#include <QTimer>
#include <QVBoxLayout>
#include <functional>
#include "colorexpandguard.h"
#include "colorscopeslayoutrefresh.h"
namespace Unbound {
// Exclusive, reversible ownership of Resolve's global native Scopes renderer.
// Page adapters supply visibility policy; native ownership and guards stay here.
class NativeScopesContent : public QObject {
public:
 explicit NativeScopesContent(QObject *owner):QObject(owner){}
 ~NativeScopesContent() override { if(m_dialog && !m_stopping)stop(); delete m_expandGuard; delete m_layoutRefresh; }
 std::function<void()> dockRequested;
 std::function<bool()> reacquire;
 static QWidget *content(QWidget *slot) {
  auto content=qobject_cast<QWidget *>(slot->property("unboundNativeScopes").value<QObject *>());
  return content ? content : slot;
 }
 QString adopt(QWidget *slot,QWidget *dialog,QWidget *host,QWidget *page,bool outsideVisible) {
  if(!slot || !dialog || !host || !page || !dialog->isWindow()
      || !dialog->findChild<QAbstractButton *>("btnHide"))
   return QStringLiteral("The native Scopes window is unavailable or already in use.");
  m_slot=slot;m_dialog=dialog;m_host=host;m_page=page;m_parent=dialog->parentWidget();
  m_outsideVisible=outsideVisible;m_suspended=false;m_stopping=false;m_dockQueued=false;
  m_flags=dialog->windowFlags();m_geometry=dialog->geometry();m_minimum=dialog->minimumSize();
  m_showWithoutActivating=dialog->testAttribute(Qt::WA_ShowWithoutActivating);
  dialog->setAttribute(Qt::WA_ShowWithoutActivating);
  dialog->setParent(host,Qt::Widget);
  dialog->setMinimumSize(0,0);
  if(!host->layout()) {auto layout=new QVBoxLayout(host);layout->setContentsMargins(0,0,0,0);}
  host->layout()->addWidget(dialog);
  slot->setProperty("unboundNativeScopes",QVariant::fromValue(static_cast<QObject *>(dialog)));
  disconnect(m_destroyed);
  m_destroyed=connect(dialog,&QObject::destroyed,this,[this]{if(m_slot)m_slot->setProperty("unboundNativeScopes",QVariant());});
  dialog->show();dialog->installEventFilter(this);
  if(slot!=page)m_expandGuard=new ColorExpandGuard(slot,this);
  m_layoutRefresh=new ColorScopesLayoutRefresh(dialog,host,this);
  return {};
 }
 bool intact()const{return m_slot && m_host && (m_suspended || (m_dialog && m_dialog->parentWidget()==m_host));}
 bool suspend() {
  if(m_suspended)return true;
  return release(false);
 }
 bool resume() {
  if(!m_suspended)return true;
  const auto acquire=reacquire;return acquire && acquire();
 }
 bool stop() {
  // Native use on another page belongs to Resolve, even if Color is retired.
  if(m_suspended){m_stopping=true;return true;}
  return release(true);
 }
private:
 bool release(bool docking) {
  if(!intact())return false;
  auto hide=m_dialog->findChild<QAbstractButton *>("btnHide");
  if((docking || !m_outsideVisible) && hide && !hide->isEnabled())return false;
  m_stopping=true;m_dialog->removeEventFilter(this);
  // Resolve updates its global Scopes visibility on the native hide path.
  // Close while still embedded/visible: reparenting or hiding the host first
  // makes btnHide a no-op for that transition and the next page reopens Scopes.
  if(docking || !m_outsideVisible) {
   if(hide)hide->click();
   else if(auto dialog=qobject_cast<QDialog *>(m_dialog.data()))dialog->reject();
   else m_dialog->close();
   if(!m_dialog || !m_dialog->isHidden()) {
    if(m_dialog)m_dialog->installEventFilter(this);
    m_stopping=false;return false;
   }
  }
  delete m_layoutRefresh;m_layoutRefresh=nullptr;
  delete m_expandGuard;m_expandGuard=nullptr;
  m_slot->setProperty("unboundNativeScopes",QVariant());
  m_host->layout()->removeWidget(m_dialog);
  m_dialog->setParent(m_parent,m_flags);m_dialog->setMinimumSize(m_minimum);
  m_dialog->setGeometry(m_geometry);
  if(!docking && m_outsideVisible) {
   m_dialog->setAttribute(Qt::WA_ShowWithoutActivating);m_dialog->show();
   // Native Show restores its own last position, which may now be the
   // embedded (0,0). Apply the retained top-level placement after that handler.
   m_dialog->setGeometry(m_geometry);
   m_dialog->setAttribute(Qt::WA_ShowWithoutActivating,m_showWithoutActivating);
   m_suspended=true;return true;
  }
  m_dialog->setAttribute(Qt::WA_ShowWithoutActivating,m_showWithoutActivating);
  if(m_dialog && m_dialog->parentWidget()==m_parent && !m_dialog->isVisible()) {
   m_suspended=!docking;return true;
  }
  if(m_dialog) {
   m_dialog->setAttribute(Qt::WA_ShowWithoutActivating);
   m_dialog->setParent(m_host,Qt::Widget);m_dialog->setMinimumSize(0,0);
   m_host->layout()->addWidget(m_dialog);
   m_slot->setProperty("unboundNativeScopes",QVariant::fromValue(static_cast<QObject *>(m_dialog.data())));
   m_dialog->show();m_dialog->installEventFilter(this);
   if(m_slot!=m_page)m_expandGuard=new ColorExpandGuard(m_slot,this);
   m_layoutRefresh=new ColorScopesLayoutRefresh(m_dialog,m_host,this);
  }
  m_stopping=false;return false;
 }
protected:
 bool eventFilter(QObject *,QEvent *event)override {
  if(m_stopping)return false;
  const bool close=event->type()==QEvent::Close || (event->type()==QEvent::KeyPress
   && static_cast<QKeyEvent *>(event)->key()==Qt::Key_Escape);
  if(close || event->type()==QEvent::Hide) {
   m_dockQueued=m_dockQueued || close;
   if(!m_queued){m_queued=true;QTimer::singleShot(0,this,[this]{
    m_queued=false;
    if(m_stopping || !m_dialog)return;
    if(m_dockQueued || (m_dialog->isHidden() && m_host && m_host->isVisible() && m_page && m_page->isVisible())) {
     m_dockQueued=false;
     const auto request=dockRequested;if(request)request();
    }
   });}
   if(close){event->ignore();return true;}
  }
  return false;
 }
private:
 QPointer<QWidget> m_slot,m_host,m_page,m_parent;
 QPointer<QWidget> m_dialog;
 Qt::WindowFlags m_flags;QRect m_geometry;QSize m_minimum;
 ColorExpandGuard *m_expandGuard=nullptr;
 ColorScopesLayoutRefresh *m_layoutRefresh=nullptr;
 QMetaObject::Connection m_destroyed;
 bool m_outsideVisible=false,m_suspended=false;
 bool m_showWithoutActivating=false,m_stopping=false,m_queued=false,m_dockQueued=false;
};
}
