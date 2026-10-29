// SPDX-License-Identifier: MIT
#include <QApplication>
#include <QAbstractButton>
#include <QAction>
#include <QDateTime>
#include <QDir>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QMenu>
#include <QMetaMethod>
#include <QSaveFile>
#include <QSet>
#include <QStackedWidget>
#include <qt_windows.h>

extern "C" __declspec(dllexport) DWORD WINAPI UnboundStart(void *) {
    if(!qApp)return 0;
    return QMetaObject::invokeMethod(qApp,[] {
        QJsonArray widgets,actions;
        QSet<QAction *> seen;
        for(auto w:QApplication::allWidgets()) {
            bool color=false;
            for(auto p=w;p;p=p->parentWidget())
                if(p->objectName()=="WStackPage_Color" || p->objectName().startsWith("unboundFloating_"))color=true;
            const auto name=w->objectName();
            auto button=qobject_cast<QAbstractButton *>(w);
            if(color && (button || name.contains("LightBox",Qt::CaseInsensitive)
                || name.contains("Player") || name=="m_pColorPanel" || name=="m_pTimeLineFrame")) {
                QJsonObject row{{"name",name},{"class",w->metaObject()->className()},
                    {"parent",w->parentWidget()?w->parentWidget()->objectName():QString()},
                    {"window",w->window()->objectName()},{"visible",w->isVisible()},
                    {"hidden",w->isHidden()},{"enabled",w->isEnabled()}};
                if(button) {row.insert("checked",button->isChecked());row.insert("checkable",button->isCheckable());row.insert("tooltip",button->toolTip());}
                if(auto stack=qobject_cast<QStackedWidget *>(w))row.insert("currentIndex",stack->currentIndex());
                QJsonArray methods;
                for(int i=QWidget::staticMetaObject.methodCount();i<w->metaObject()->methodCount();++i) {
                    const auto m=w->metaObject()->method(i);
                    methods.append(QJsonObject{{"signature",QString::fromLatin1(m.methodSignature())},{"type",int(m.methodType())}});
                }
                row.insert("methods",methods);widgets.append(row);
            }
            if(auto menu=qobject_cast<QMenu *>(w))for(auto a:menu->actions()) {
                if(seen.contains(a))continue;seen.insert(a);
                const auto text=a->text();
                if(!text.contains("Lightbox",Qt::CaseInsensitive) && !text.contains("Play",Qt::CaseInsensitive)
                    && !text.contains("Stop",Qt::CaseInsensitive))continue;
                actions.append(QJsonObject{{"name",a->objectName()},{"text",text},{"menu",menu->title()},
                    {"enabled",a->isEnabled()},{"checked",a->isChecked()},{"shortcut",a->shortcut().toString()}});
            }
        }
        const auto path=QDir::tempPath()+"/Unbound-color-mode";QDir().mkpath(path);
        QSaveFile file(path+"/"+QDateTime::currentDateTimeUtc().toString("yyyyMMdd-HHmmss-zzz")+".json");
        if(file.open(QIODevice::WriteOnly)){file.write(QJsonDocument(QJsonObject{{"widgets",widgets},{"actions",actions}}).toJson());file.commit();}
    },Qt::QueuedConnection)?1:0;
}
