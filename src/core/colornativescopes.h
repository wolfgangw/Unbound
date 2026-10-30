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
// Resolve's independent Scopes renderer belongs to its native floating dialog,
// not the single docked scope. Retain that dialog and its native registration.
class ColorNativeScopes final : public QObject {
public:
 explicit ColorNativeScopes(QObject *owner):QObject(owner){}
 ~ColorNativeScopes() override { if(m_dialog && !m_stopping)stop(); delete m_expandGuard; delete m_layoutRefresh; }
 std::function<void()> dockRequested;
 static QWidget *content(QWidget *slot) {
  auto content=qobject_cast<QWidget *>(slot->property("unboundNativeScopes").value<QObject *>());
  return content ? content : slot;
 }
 QString start(QWidget *slot,QWidget *host,QWidget *page) {
  auto main=slot->window();
  // The native Scopes shell is a QWidget on macOS and a QDialog on Windows.
  // Its object identity and native controls define the contract, not that subtype.
  auto dialog=main->findChild<QWidget *>("UiWaveformWindow");
  if(!dialog || !dialog->isVisible()) {
   QAbstractButton *expand=nullptr;
   for(auto button:slot->findChildren<QAbstractButton *>("btnUndock")) {
    bool hidden=false;
    for(auto w=static_cast<QWidget *>(button);w && w!=slot;w=w->parentWidget())hidden=hidden || w->isHidden();
    if(!hidden && button->isEnabled()){expand=button;break;}
   }
   if(!expand)return QStringLiteral("The native Scopes expansion control is unavailable.");
   expand->click();
   dialog=main->findChild<QWidget *>("UiWaveformWindow");
  }
  if(!dialog || !dialog->isVisible() || !dialog->findChild<QAbstractButton *>("btnHide"))
   return QStringLiteral("DaVinci Resolve did not provide its native floating Scopes window.");
  m_slot=slot;m_dialog=dialog;m_host=host;m_page=page;m_parent=dialog->parentWidget();
  m_flags=dialog->windowFlags();m_geometry=dialog->geometry();m_minimum=dialog->minimumSize();
  m_showWithoutActivating=dialog->testAttribute(Qt::WA_ShowWithoutActivating);
  dialog->setAttribute(Qt::WA_ShowWithoutActivating);
  dialog->setParent(host,Qt::Widget);
  dialog->setMinimumSize(0,0);
  auto layout=new QVBoxLayout(host);layout->setContentsMargins(0,0,0,0);layout->addWidget(dialog);
  slot->setProperty("unboundNativeScopes",QVariant::fromValue(static_cast<QObject *>(dialog)));
  connect(dialog,&QObject::destroyed,this,[this]{if(m_slot)m_slot->setProperty("unboundNativeScopes",QVariant());});
  dialog->show();dialog->installEventFilter(this);
  m_expandGuard=new ColorExpandGuard(slot,this);
  m_layoutRefresh=new ColorScopesLayoutRefresh(dialog,host,this);
  return {};
 }
 bool intact()const{return m_slot && m_host && m_dialog && m_dialog->parentWidget()==m_host;}
 bool stop() {
  if(!intact())return false;
  auto hide=m_dialog->findChild<QAbstractButton *>("btnHide");
  if(hide && !hide->isEnabled())return false;
  m_stopping=true;m_dialog->removeEventFilter(this);
  delete m_layoutRefresh;m_layoutRefresh=nullptr;
  delete m_expandGuard;m_expandGuard=nullptr;
  m_slot->setProperty("unboundNativeScopes",QVariant());
  m_host->layout()->removeWidget(m_dialog);
  m_dialog->setParent(m_parent,m_flags);m_dialog->setMinimumSize(m_minimum);
  m_dialog->setGeometry(m_geometry);m_dialog->setAttribute(Qt::WA_ShowWithoutActivating,m_showWithoutActivating);
  // The native action retires the independent renderer and returns the docked
  // selector. Its public button works even after reparenting has hidden it.
  if(hide)hide->click();
  else if(auto dialog=qobject_cast<QDialog *>(m_dialog.data()))dialog->reject();
  else m_dialog->close();
  if(m_dialog && m_dialog->parentWidget()==m_parent && !m_dialog->isVisible())return true;
  if(m_dialog) {
   m_dialog->setAttribute(Qt::WA_ShowWithoutActivating);
   m_dialog->setParent(m_host,Qt::Widget);m_dialog->setMinimumSize(0,0);
   m_host->layout()->addWidget(m_dialog);
   m_slot->setProperty("unboundNativeScopes",QVariant::fromValue(static_cast<QObject *>(m_dialog.data())));
   m_dialog->show();m_dialog->installEventFilter(this);
   m_expandGuard=new ColorExpandGuard(m_slot,this);
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
 bool m_showWithoutActivating=false,m_stopping=false,m_queued=false,m_dockQueued=false;
};
}
