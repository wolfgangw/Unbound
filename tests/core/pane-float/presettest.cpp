// SPDX-License-Identifier: MIT
#include "pagepresetmenu.h"
#include "panedecoration.h"
#include "controller.h"
#include <QApplication>
#include <QMenu>
#include <qt_windows.h>
#include "windowplacement.h"
#include <QTemporaryDir>
#include <QEventLoop>
#include <QStackedWidget>
#include <QPushButton>
#include <cstdio>
#include <cstdlib>
using namespace Unbound;
static void require(bool ok,const char *message) { if(!ok) { std::fprintf(stderr,"FAIL: %s\n",message); std::exit(1); } }
// The 025 Resolve trace additionally shows that sendEvent(editor, keyPress)
// is swallowed before the probe's application/object filters, while overrides/releases pass.
class HostApplication final : public QApplication {
public:
    using QApplication::QApplication;
    QPointer<QObject> blockedEditor;
    int blocked=0;
    bool notify(QObject *receiver,QEvent *event) override {
        if(receiver==blockedEditor && event->type()==QEvent::KeyPress) {
            ++blocked; return true;
        }
        return QApplication::notify(receiver,event);
    }
};
// Model the live Resolve trace: focus and ShortcutOverride reach the editor,
// but key presses are consumed at the popout's QWidgetWindow until a click.
class WindowKeyInterceptor final : public QObject {
public:
    QObject *target=nullptr;
    int intercepted=0;
    bool eventFilter(QObject *object,QEvent *event) override {
        if(object==target && event->type()==QEvent::KeyPress) { ++intercepted; return true; }
        return false;
    }
};
static void settle() {
    // Native activation/popup notifications arrive asynchronously from Windows;
    // draining only already-queued events can test an intermediate focus state.
    QEventLoop loop; QTimer::singleShot(20,&loop,&QEventLoop::quit); loop.exec();
}
static void click(QWidget *widget) {
    require(widget,"Missing preset menu control");
    // sendEvent does not perform the foreground activation of a real click.
    // Without it, docking the previously active floating host can transfer
    // focus to another application and correctly dismiss the popup.
    auto window=widget->window();
    if(qobject_cast<QMenu *>(window) && window->parentWidget()) window=window->parentWidget()->window();
    window->raise(); window->activateWindow(); settle();
    const auto point=widget->rect().center();
    QMouseEvent press(QEvent::MouseButtonPress,point,widget->mapToGlobal(point),Qt::LeftButton,Qt::LeftButton,Qt::NoModifier);
    QMouseEvent release(QEvent::MouseButtonRelease,point,widget->mapToGlobal(point),Qt::LeftButton,Qt::NoButton,Qt::NoModifier);
    QApplication::sendEvent(widget,&press); QApplication::sendEvent(widget,&release); settle();
}
static void context(QWidget *button) {
    button->window()->activateWindow(); settle();
    const auto point=button->rect().center();
    QContextMenuEvent event(QContextMenuEvent::Mouse,point,button->mapToGlobal(point));
    QApplication::sendEvent(button,&event); settle();
}
static QAbstractButton *control(PagePresetMenu *menu,const QString &object,const QString &text={}) {
    for(auto button:menu->findChildren<QAbstractButton *>(object))
        if(button->isVisible() && (text.isEmpty() || button->text()==text || button->accessibleName()==text)) return button;
    return nullptr;
}
static void hold(QWidget *widget,bool activate=true) {
    require(widget,"Missing hold target");
    auto target=widget->window();
    if(!activate && target->parentWidget()) target=target->parentWidget()->window();
    target->activateWindow(); settle();
    const auto point=widget->rect().center(),global=widget->mapToGlobal(point);
    QMouseEvent press(QEvent::MouseButtonPress,point,global,Qt::LeftButton,Qt::LeftButton,Qt::NoModifier);
    QApplication::sendEvent(widget,&press);
    QEventLoop loop; QTimer::singleShot(1100,&loop,&QEventLoop::quit); loop.exec();
    QMouseEvent release(QEvent::MouseButtonRelease,point,global,Qt::LeftButton,Qt::NoButton,Qt::NoModifier);
    QApplication::sendEvent(widget,&release); settle();
}
static QString presetId(const PaneSettings &settings,const QString &page,const QString &name) {
    QString id;
    for(const auto &value:settings.presets(page)) if(value.toObject().value("name").toString()==name) {
        require(id.isEmpty(),"Test expected a unique preset label"); id=value.toObject().value("id").toString();
    }
    require(!id.isEmpty(),"Test could not find the preset ID"); return id;
}
class PlacementFrame final : public QWidget {
protected:
    bool nativeEvent(const QByteArray &type,void *message,long *result) override {
        const auto msg=static_cast<MSG *>(message);
        if(msg->message==WM_NCCALCSIZE && !IsZoomed(msg->hwnd)) {
            auto rect=reinterpret_cast<RECT *>(msg->lParam);
            rect->left+=4; rect->right-=4; rect->bottom-=4;
            *result=0; return true;
        }
        return QWidget::nativeEvent(type,message,result);
    }
};
static void nativePlacementEdges() {
    if(QGuiApplication::platformName()!=QLatin1String("windows")) return;
    PlacementFrame frame; frame.setGeometry(100,100,700,500); frame.show(); settle();
    auto handle=reinterpret_cast<HWND>(frame.winId());
    for(const auto point:{QPoint(-4,0),QPoint(0,-1),QPoint(-4,-1)}) {
        SetWindowPos(handle,nullptr,point.x(),point.y(),700,500,SWP_NOZORDER|SWP_NOACTIVATE); settle();
        RECT expected{}; GetWindowRect(handle,&expected);
        const QJsonArray screens;
        const auto saved=QJsonDocument::fromJson(Platform::saveWindowPlacement(&frame,screens)).object();
        SetWindowPos(handle,nullptr,100,100,700,500,SWP_NOZORDER|SWP_NOACTIVATE); settle();
        require(Platform::restoreWindowPlacement(&frame,saved,screens),"Native edge restoration failed"); settle();
        RECT actual{}; GetWindowRect(handle,&actual);
        require(EqualRect(&expected,&actual),"Windows placement changed an edge-aligned rectangle");
    }
    frame.hide();
    std::puts("PASS: exact native left/top edge placement with Resolve-like custom client borders");
}
static void storage() {
    QTemporaryDir temporary;
    const auto path=temporary.filePath("Unbound.json");
    const QString edit="WStackPage_Conform",color="WStackPage_Color";
    QWidget sample; sample.setGeometry(70,90,400,300);
    QJsonObject pages{{edit,QJsonObject{{"panes",QJsonObject{{"frameEditPageMediaPool",QJsonObject{
        {"floating",true},{"toolbarButtonChecked",false},{"window",placementJson(sample.saveGeometry())}}}}}}}};
    QFile old(path); require(old.open(QIODevice::WriteOnly),"Cannot seed version 5 settings");
    old.write(QJsonDocument(QJsonObject{{"version",5},{"pages",pages}}).toJson()); old.close();
    PaneSettings settings(path);
    QFile migrated(path); require(migrated.open(QIODevice::ReadOnly),"Cannot read migrated settings");
    const auto root=QJsonDocument::fromJson(migrated.readAll()).object(); migrated.close();
    require(root.value("version").toInt()==7 && root.value("pages").toObject()==pages,
        "Preset migration changed existing layouts or invented presets");
    QString name; bool duplicate=false;
    require(settings.savePreset(edit,name,duplicate).isEmpty() && name=="Edit-1" && !duplicate,"First preset naming failed");
    require(settings.savePreset(edit,name,duplicate).isEmpty() && duplicate && settings.presets(edit).size()==1,
        "Identical page configuration consumed a preset slot");
    require(settings.savePreset(color,name,duplicate).isEmpty() && name=="Color-1","Preset names were not page-local");
    const auto first=settings.preset(edit,presetId(settings,edit,"Edit-1"));
    for(int i=2;i<=5;++i) {
        sample.move(70+i*20,90+i*15); settings.save("frameEditPageMediaPool",sample.saveGeometry());
        require(settings.savePreset(edit,name,duplicate).isEmpty() && !duplicate,"Could not fill five preset slots");
    }
    sample.move(240,220); settings.save("frameEditPageMediaPool",sample.saveGeometry());
    require(!settings.savePreset(edit,name,duplicate).isEmpty() && settings.presets(edit).size()==5,"Preset limit was not enforced");
    require(settings.deletePreset(edit,presetId(settings,edit,"Edit-3")).isEmpty(),"Cannot delete preset");
    require(settings.savePreset(edit,name,duplicate).isEmpty() && name=="Edit-6","Deletion reused a name or prevented saving");
    const auto colorBefore=settings.pageConfiguration(color);
    require(settings.activatePreset(edit,presetId(settings,edit,"Edit-1")).isEmpty() && settings.pageConfiguration(edit)==first,
        "Preset recall did not restore the saved configuration");
    require(settings.pageConfiguration(color)==colorBefore && settings.presets(edit).size()==5,
        "Preset recall modified another page or its archived presets");
    PaneSettings reloaded(path);
    require(reloaded.preset(edit,presetId(reloaded,edit,"Edit-1"))==first && reloaded.presets(edit).size()==5 && reloaded.presets(color).size()==1,
        "Saved presets did not survive disk reload");
    require(reloaded.activePreset(edit)==presetId(reloaded,edit,"Edit-1") && !reloaded.presetDirty(edit,presetId(reloaded,edit,"Edit-1")),
        "Active preset did not persist or was dirty immediately after recall");
    reloaded.saveFloating("frameEditPageMediaPool",false);
    require(reloaded.activePreset(edit)==presetId(reloaded,edit,"Edit-1") && reloaded.presetDirty(edit,presetId(reloaded,edit,"Edit-1")),
        "Manual changes lost active identity or failed to mark it dirty");
    require(reloaded.updatePreset(edit,presetId(reloaded,edit,"Edit-1")).isEmpty() && !reloaded.presetDirty(edit,presetId(reloaded,edit,"Edit-1")) &&
        reloaded.presets(edit).size()==5 && reloaded.preset(edit,presetId(reloaded,edit,"Edit-2"))==settings.preset(edit,presetId(settings,edit,"Edit-2")),
        "Update did not replace only the selected preset in place");
    require(reloaded.deletePreset(edit,presetId(reloaded,edit,"Edit-1")).isEmpty() && reloaded.activePreset(edit).isEmpty(),
        "Deleting the active preset retained its identity");
    QFile invalid(temporary.filePath("future.json")); require(invalid.open(QIODevice::WriteOnly),"Cannot seed future format");
    const QByteArray future="{\"version\":99,\"pages\":{}}"; invalid.write(future); invalid.close();
    PaneSettings unsupported(invalid.fileName());
    require(!unsupported.savePreset(edit,name,duplicate).isEmpty(),"Future settings format was overwritten by preset save");
    require(invalid.open(QIODevice::ReadOnly) && invalid.readAll()==future,"Unsupported settings were modified"); invalid.close();
    QFile blocking(temporary.filePath("not-a-directory")); require(blocking.open(QIODevice::WriteOnly),"Cannot seed failed-write test"); blocking.close();
    PaneSettings unwritable(blocking.fileName()+"/presets.json");
    require(!unwritable.savePreset(edit,name,duplicate).isEmpty() && unwritable.presets(edit).isEmpty(),
        "Failed preset write changed the in-memory preset list");
    std::puts("PASS: v5 migration, page-local naming, duplicate suppression, five-preset limit, delete/save, immutable recall and disk reload");
}
static void presetIdentity() {
    QTemporaryDir directory; const auto path=directory.filePath("identity.json");
    const QString page="WStackPage_Conform";
    PaneSettings settings(path); QWidget sample;
    QStringList ids; QList<QJsonObject> configurations;
    for(int i=0;i<5;++i) {
        sample.setGeometry(50+i*20,70,400,300); settings.save("frameEditPageMediaPool",sample.saveGeometry());
        QString name; bool duplicate=false;
        require(settings.savePreset(page,name,duplicate).isEmpty(),"Cannot create identity fixture");
        ids.append(settings.activePreset(page)); configurations.append(settings.pageConfiguration(page));
    }
    const auto active=settings.activePreset(page); const auto working=settings.pageConfiguration(page);
    for(const auto &id:ids) require(settings.renamePreset(page,id,"doodoo").isEmpty(),"Duplicate name was rejected");
    require(settings.activePreset(page)==active && settings.pageConfiguration(page)==working && settings.presets(page).size()==5,
        "Rename changed the active configuration or merged duplicate names");
    for(int i=0;i<ids.size();++i) require(settings.activatePreset(page,ids[i]).isEmpty() &&
        settings.pageConfiguration(page)==configurations[i],"Duplicate-name recall selected the wrong preset");
    settings.saveFloating("frameEditPageMediaPool",true);
    const auto untouched=settings.preset(page,ids[1]);
    require(settings.updatePreset(page,ids[0]).isEmpty() && settings.preset(page,ids[1])==untouched &&
        settings.preset(page,ids[0])!=configurations[0],"Duplicate-name update changed the wrong preset");
    require(!settings.renamePreset(page,ids[0],"   ").isEmpty() && !settings.renamePreset(page,ids[0],QString(129,'x')).isEmpty(),
        "Blank or overlong name accepted");
    require(settings.renamePreset(page,ids[0],"  & wow  ").isEmpty() && settings.presetName(page,ids[0])=="& wow",
        "Name normalization failed");
    require(settings.deletePreset(page,ids[2]).isEmpty() && settings.presets(page).size()==4 &&
        settings.preset(page,ids[2]).isEmpty() && settings.preset(page,ids[1])==untouched,"Duplicate-name deletion targeted the wrong preset");
    PaneSettings reloaded(path);
    require(reloaded.presets(page)==settings.presets(page) && reloaded.activePreset(page)==ids[0],"Preset IDs were not stable on reload");
    // Version 6 used names as identities. Migrate its active name and every
    // archived snapshot without changing the working layout or generated counter.
    QFile file(path); require(file.open(QIODevice::ReadOnly),"Cannot read identity fixture");
    auto root=QJsonDocument::fromJson(file.readAll()).object(); file.close();
    auto pages=root.value("pages").toObject(); auto state=pages.value(page).toObject();
    auto saved=state.value("presets").toArray();
    for(int i=0;i<saved.size();++i) {
        auto item=saved[i].toObject(); item.remove("id"); item.insert("name",QString("Legacy-%1").arg(i)); saved[i]=item;
    }
    state.insert("presets",saved); state.insert("activePreset","Legacy-1"); pages.insert(page,state);
    root.insert("version",6); root.insert("pages",pages);
    require(file.open(QIODevice::WriteOnly|QIODevice::Truncate),"Cannot seed v6 fixture"); file.write(QJsonDocument(root).toJson()); file.close();
    PaneSettings migrated(path);
    require(migrated.readError().isEmpty() && migrated.pageConfiguration(page)==settings.pageConfiguration(page) &&
        migrated.activePreset(page)==presetId(migrated,page,"Legacy-1"),"v6 migration changed layout or lost active identity");
    for(int i=0;i<saved.size();++i) require(migrated.preset(page,presetId(migrated,page,QString("Legacy-%1").arg(i)))==
        saved[i].toObject().value("configuration").toObject(),"v6 migration changed an archived snapshot");
    const auto migratedIds=migrated.presets(page); PaneSettings migratedAgain(path);
    require(migratedAgain.presets(page)==migratedIds,"Migration regenerated persisted IDs");
    std::puts("PASS: five duplicate names, independent ID recall/update/delete, stable reload and v6 active-ID migration");
}
class PopupEvents final : public QObject {
public:
    int hides=0,shows=0;
    bool eventFilter(QObject *,QEvent *event) override {
        if(event->type()==QEvent::Hide) ++hides;
        if(event->type()==QEvent::Show) ++shows;
        return false;
    }
};
struct Panel {
    QWidget *pane=nullptr;
    QPushButton *toggle=nullptr;
    QLabel *content=nullptr;
};
static Panel panel(QWidget *root,QBoxLayout *layout,const QString &name,const QString &toggleName,
    const QString &headerName,const QString &menuName) {
    Panel result;
    result.toggle=new QPushButton("Toggle",root); result.toggle->setObjectName(toggleName);
    result.toggle->setCheckable(true); result.toggle->setChecked(true); layout->addWidget(result.toggle);
    result.pane=new QWidget(root); result.pane->setObjectName(name); layout->addWidget(result.pane,1);
    auto paneLayout=new QVBoxLayout(result.pane);
    QWidget *body=result.pane;
    if(name=="m_pWidgetGallery") {
        body=new QWidget(result.pane); paneLayout->addWidget(body); paneLayout=new QVBoxLayout(body);
    }
    auto header=new QWidget(body); header->setObjectName(headerName); paneLayout->addWidget(header);
    auto row=new QHBoxLayout(header); row->addWidget(new QLabel("Panel",header)); row->addStretch();
    auto menu=new QToolButton(header); menu->setText("..."); menu->setObjectName(menuName); row->addWidget(menu);
    result.content=new QLabel("Native panel content",body); paneLayout->addWidget(result.content,1);
    QObject::connect(result.toggle,&QPushButton::clicked,result.pane,[pane=result.pane](bool checked) { pane->setVisible(checked); });
    return result;
}
int main(int argc,char **argv) {
    std::setvbuf(stdout,nullptr,_IONBF,0);
    HostApplication app(argc,argv); nativePlacementEdges(); storage(); presetIdentity();
    QTemporaryDir directory;
    qputenv("UNBOUND_SETTINGS_FILE",directory.filePath("presets.json").toUtf8());
    PlacementFrame main;
    auto theme=main.palette(); theme.setColor(QPalette::Window,QColor("#202024"));
    theme.setColor(QPalette::WindowText,QColor("#dddddd")); main.setPalette(theme);
    main.setObjectName("UiMainWindow"); main.setWindowTitle("Unbound preset fixture");
    auto mainLayout=new QVBoxLayout(&main);
    auto stack=new QStackedWidget(&main); stack->setObjectName("widgetStack_Panel"); mainLayout->addWidget(stack);
    auto edit=new QWidget(stack); edit->setObjectName("WStackPage_Conform"); stack->addWidget(edit);
    auto editLayout=new QVBoxLayout(edit);
    auto regions=new QWidget(edit); regions->setObjectName("frameEditPageWidgetsContainer"); editLayout->addWidget(regions);
    auto editColumns=new QVBoxLayout(regions);
    const auto pool=panel(regions,editColumns,"frameEditPageMediaPool","btnEditToggleMediaPoolInSSM","frameMediaPoolTitleBarTop","btnMediaPoolOptions");
    const auto effects=panel(regions,editColumns,"frameEditPageEffectsLibrary","btnEditToggleEffectsLibraryInSSM","frameEffectTopBar","btnOptions");
    auto color=new QWidget(stack); color->setObjectName("WStackPage_Color"); stack->addWidget(color);
    auto colorLayout=new QVBoxLayout(color);
    const auto gallery=panel(color,colorLayout,"m_pWidgetGallery","btnColorToggleGallery","frameTopColorGallery","btnOptions");
    auto audio=new QWidget(stack); audio->setObjectName("WStackPage_Audio"); stack->addWidget(audio);
    auto audioLayout=new QVBoxLayout(audio);
    const auto groups=panel(audio,audioLayout,"frameAudioPanelGroupsContainer","btnAudioPanelGroupsInSSM","frameHeaderTitle","m_BtnMenu");
    main.setGeometry(30,40,650,450); main.show(); settle();
    RECT initialNative{};
    if(QGuiApplication::platformName()==QLatin1String("windows")) {
        const auto handle=reinterpret_cast<HWND>(main.winId());
        SetWindowPos(handle,nullptr,-4,0,650,450,SWP_NOZORDER|SWP_NOACTIVATE); settle();
        GetWindowRect(handle,&initialNative);
    }
    require(startController(),"Cannot start preset controller"); settle();
    auto poolButton=pool.pane->findChild<QToolButton *>("resolvePaneUndockButton");
    require(poolButton,"Controller did not decorate Media Pool");
    auto popup=[&]() {
        static int checkNumber=0; ++checkNumber;
        auto menu=main.findChild<PagePresetMenu *>("unboundPresetMenu");
        if(!menu || !menu->isVisible()) std::fprintf(stderr,"Popup check %d; menu exists=%d\n",checkNumber,menu!=nullptr);
        require(menu && menu->isVisible(),"Preset popup did not open or closed unexpectedly"); return menu;
    };
    context(poolButton); auto menu=popup();
    require(control(menu,"unboundPresetSave")->text()=="Save as new preset","New preset save label is incorrect");
    const auto anchor=poolButton->mapToGlobal(QPoint(0,poolButton->height()));
    require(menu->pos().y()==anchor.y() && !menu->geometry().intersects(QRect(poolButton->mapToGlobal(QPoint()),poolButton->size())),
        "Popup overlaps its button instead of starting directly below it");
    require(!control(menu,"unboundPresetDockAll"),"Dock all appears with no floating panels");
    const auto background=menu->grab().toImage().pixelColor(2,menu->height()/2);
    require(background.alpha()==255 && background.rgb()==main.palette().color(QPalette::Window).rgb(),
        "Popout body is transparent or has the wrong theme background");
    const auto originalMainGeometry=main.geometry();
    require(poolButton->isChecked()==false,"Right-click toggled docking");
    click(control(menu,"unboundPresetSave")); popup();
    require(control(menu,"unboundPresetRecall","Edit-1"),"Save did not update the open menu");
    require(control(menu,"unboundPresetRecall","Edit-1")->property("activePreset").toBool() &&
        !control(menu,"unboundPresetUpdate"),"Saved preset is not highlighted and clean");
    click(control(menu,"unboundPresetSave")); popup();
    require(main.findChild<QLabel *>("unboundPresetStatus")->text().contains("Already saved"),"Duplicate save was not reported");
    menu->hide(); click(poolButton); settle();
    require(poolButton->isChecked() && pool.content->window()!=&main,"Pool did not float");
    auto floating=pool.content->window(); floating->setGeometry(120,150,440,320); settle();
    const auto savedGeometry=floating->geometry();
    main.setGeometry(70,80,680,460); settle();
    const auto savedMainGeometry=main.geometry();
    context(poolButton); menu=popup();
    require(control(menu,"unboundPresetDockAll"),"Dock all is unavailable with a floating panel");
    click(control(menu,"unboundPresetSave")); popup();
    require(control(menu,"unboundPresetRecall","Edit-2"),"Floating configuration was not saved");
    if(QGuiApplication::platformName()==QLatin1String("windows")) {
        const auto handle=reinterpret_cast<HWND>(floating->winId());
        SendMessage(handle,WM_NCLBUTTONDOWN,HTRIGHT,0);
        require(!menu->isVisible(),"Native host border resize did not dismiss the popout");
        SendMessage(handle,WM_CANCELMODE,0,0); settle();
        require(floating->geometry()==savedGeometry,"Cancelled dismissal test changed floating geometry");
        context(poolButton); menu=popup();
        // Exercise the native-child subclass path too: its border press never
        // becomes a Qt NonClientAreaMouseButtonPress on the floating host.
        {
            QWidget child(floating); child.setGeometry(0,0,50,30);
            child.setAttribute(Qt::WA_NativeWindow); child.show(); settle();
            const auto childHandle=reinterpret_cast<HWND>(child.winId());
            const auto point=floating->mapToGlobal(QPoint(25,1));
            require(SendMessage(childHandle,WM_NCHITTEST,0,MAKELPARAM(point.x(),point.y()))==HTTOP,
                "Native-child dismissal fixture did not reach a resize edge");
            SendMessage(childHandle,WM_NCLBUTTONDOWN,HTTOP,MAKELPARAM(point.x(),point.y()));
            require(!menu->isVisible(),"Native-child border resize did not dismiss the popout");
            SendMessage(handle,WM_CANCELMODE,0,0); settle();
        }
        context(poolButton); menu=popup();
        const auto mainHandle=reinterpret_cast<HWND>(main.winId());
        SendMessage(mainHandle,WM_ENTERSIZEMOVE,0,0);
        require(!menu->isVisible(),"Main-window native move/resize did not dismiss the popout");
        SendMessage(mainHandle,WM_EXITSIZEMOVE,0,0); settle();
        context(poolButton); menu=popup();
    }
    PopupEvents popupEvents; menu->installEventFilter(&popupEvents);
    const auto realRecall=menu->recall; int appliedRecalls=0; bool internalDismissal=false;
    menu->recall=[&](const QString &name,bool &applied) {
        const auto error=realRecall(name,applied);
        if(applied) ++appliedRecalls;
        if(applied && internalDismissal) {
            QEvent deactivated(QEvent::WindowDeactivate);
            QApplication::sendEvent(menu,&deactivated);
            if(QGuiApplication::platformName()==QLatin1String("windows")) {
                QMenu nativePopup(menu); nativePopup.addAction("Native action");
                nativePopup.popup(menu->pos()+QPoint(10,10)); nativePopup.hide();
            }
            require(menu->isVisible(),"Native popup cleanup dismissed the persistent preset popout");
        }
        return error;
    };
    auto inactive=control(menu,"unboundPresetRecall","Edit-1");
    const auto inactiveId=inactive->property("presetId").toString();
    WindowKeyInterceptor interceptor; interceptor.target=menu->windowHandle();
    qApp->installEventFilter(&interceptor);
    hold(inactive,false); popup();
    auto editor=menu->findChild<PresetNameEdit *>("unboundPresetNameEdit");
    for(auto candidate:menu->findChildren<PresetNameEdit *>()) if(candidate->isVisible()) editor=candidate;
    require(editor && editor->isVisible() && appliedRecalls==0,"Long hold recalled instead of editing");
    app.blockedEditor=editor;
    require(editor->hasFocus() && QApplication::focusWidget()==editor && menu->isActiveWindow(),
        "Long hold selected text without activating keyboard input");
    if(QGuiApplication::platformName()==QLatin1String("windows"))
        require(GetAncestor(GetFocus(),GA_ROOT)==reinterpret_cast<HWND>(menu->winId()),
            "Native keyboard focus stayed outside the name editor's window");
    auto routeKey=[&](int key,QString text=QString(),Qt::KeyboardModifiers modifiers=Qt::NoModifier) {
        auto focused=QApplication::focusWidget(); require(focused==editor,"Keyboard input is not routed to the editor");
        QKeyEvent event(QEvent::KeyPress,key,modifiers,text);
        QApplication::sendEvent(menu->windowHandle(),&event); settle();
        require(interceptor.intercepted==0,"Rename keys escaped into the host's window interception");
    };
    routeKey(Qt::Key_Left);
    require(editor->selectedText().isEmpty() && editor->cursorPosition()==0,"Left did not collapse selection to the start");
    editor->selectAll(); routeKey(Qt::Key_Right);
    require(editor->selectedText().isEmpty() && editor->cursorPosition()==editor->text().size(),
        "Right did not collapse selection to the end");
    routeKey(Qt::Key_Left,QString(),Qt::ShiftModifier);
    require(editor->selectedText()==editor->text().right(1),"Modified arrow selection behavior changed");
    editor->selectAll(); routeKey(Qt::Key_X,"x");
    require(editor->text()=="x","Typing did not replace the selected name without another click");
    editor->setText("doodoo"); QKeyEvent submit(QEvent::KeyPress,Qt::Key_Return,Qt::NoModifier);
    routeKey(Qt::Key_Return); popup();
    require(app.blocked==0,"Rename sent keys back through the host's blocked editor dispatch");
    app.blockedEditor=nullptr;
    require(!editor->isVisible() && control(menu,"unboundPresetRecall","doodoo")==inactive &&
        control(menu,"unboundPresetRecall","Edit-2")->property("activePreset").toBool() && appliedRecalls==0 && poolButton->isChecked(),
        "Enter on an inactive preset renamed and recalled it");
    hold(inactive); editor->setText("cancelled"); QKeyEvent cancel(QEvent::KeyPress,Qt::Key_Escape,Qt::NoModifier);
    QApplication::sendEvent(editor,&cancel); settle(); popup();
    require(!editor->isVisible() && inactive->text()=="doodoo" && appliedRecalls==0,"Escape did not cancel rename without recall/dismissal");
    hold(inactive); editor->setText("Edit-1"); QApplication::sendEvent(editor,&submit); settle();
    require(inactive->property("presetId").toString()==inactiveId && appliedRecalls==0,"Renaming replaced the preset identity");
    QPointer<QAbstractButton> retainedEdit1=control(menu,"unboundPresetRecall","Edit-1");
    QPointer<QAbstractButton> retainedEdit2=control(menu,"unboundPresetRecall","Edit-2");
    for(int repeat=0;repeat<3;++repeat) click(retainedEdit2);
    require(appliedRecalls==0 && retainedEdit2 && popupEvents.hides==0 && popupEvents.shows==0,
        "Repeated clean recall performed restoration or disturbed the popup");
    internalDismissal=true;
    floating->move(300,250); settle();
    main.setGeometry(90,100,640,420); settle();
    click(control(menu,"unboundPresetRecall","Edit-2")); popup();
    internalDismissal=false;
    require(appliedRecalls==1,"Dirty active preset recall was incorrectly skipped");
    require(retainedEdit1 && retainedEdit2 && control(menu,"unboundPresetRecall","Edit-2")==retainedEdit2,
        "Recall replaced the menu rows instead of updating them in place");
    require(pool.content->window()==floating && floating->geometry()==savedGeometry,
        "Recall rebuilt an already-floating panel or failed to restore its geometry");
    if(main.geometry()!=savedMainGeometry || pool.toggle->isChecked()) std::fprintf(stderr,
        "Main got %d,%d %dx%d expected %d,%d %dx%d toggle=%d\n",main.x(),main.y(),main.width(),main.height(),
        savedMainGeometry.x(),savedMainGeometry.y(),savedMainGeometry.width(),savedMainGeometry.height(),pool.toggle->isChecked());
    require(main.geometry()==savedMainGeometry && !pool.toggle->isChecked(),
        "Recall did not restore main-window geometry and the native pane toggle");
    click(control(menu,"unboundPresetRecall","Edit-1")); popup();
    require(!poolButton->isChecked() && pool.content->window()==&main,"Recall did not dock the changed panel");
    require(retainedEdit1 && retainedEdit2,"Switching presets destroyed retained menu rows");
    require(popupEvents.hides==0 && popupEvents.shows==0,"Switching presets hid/reopened the popup");
    if(main.geometry()!=originalMainGeometry || !pool.toggle->isChecked()) std::fprintf(stderr,
        "Docked main got %d,%d %dx%d expected %d,%d %dx%d toggle=%d\n",main.x(),main.y(),main.width(),main.height(),
        originalMainGeometry.x(),originalMainGeometry.y(),originalMainGeometry.width(),originalMainGeometry.height(),pool.toggle->isChecked());
    require(main.geometry()==originalMainGeometry && pool.toggle->isChecked(),
        "Docked preset did not restore main geometry or native visibility");
    if(QGuiApplication::platformName()==QLatin1String("windows")) {
        RECT restored{}; GetWindowRect(reinterpret_cast<HWND>(main.winId()),&restored);
        require(EqualRect(&restored,&initialNative),"Preset recall introduced a gap at the left/top display edge");
    }
    click(control(menu,"unboundPresetRecall","Edit-2")); popup();
    require(poolButton->isChecked() && pool.content->window()->geometry()==savedGeometry,"Recall did not restore floating geometry");
    click(control(menu,"unboundPresetDelete","Delete Edit-1")); popup();
    require(!control(menu,"unboundPresetRecall","Edit-1"),"Delete did not update the open menu");
    pool.content->window()->move(350,280); settle();
    click(control(menu,"unboundPresetSave")); popup();
    require(control(menu,"unboundPresetRecall","Edit-3"),"Delete-then-save sequence failed without dismissal");
    menu->hide(); pool.content->window()->move(320,260); settle();
    const auto updatedGeometry=pool.content->window()->geometry();
    context(poolButton); menu=popup();
    require(control(menu,"unboundPresetRecall","Edit-3")->property("activePreset").toBool() &&
        control(menu,"unboundPresetUpdate","Update Edit-3"),"Dirty active preset has no update button");
    click(control(menu,"unboundPresetUpdate","Update Edit-3")); popup();
    require(!control(menu,"unboundPresetUpdate"),"Update left the preset dirty");
    click(control(menu,"unboundPresetDockAll")); popup();
    require(!poolButton->isChecked() && pool.toggle->isChecked() &&
        !control(menu,"unboundPresetRecall","Edit-3")->property("activePreset").toBool(),
        "Dock all did not dock/show panels and clear active identity");
    require(!control(menu,"unboundPresetDockAll"),"Dock all remained visible after docking every panel");
    click(control(menu,"unboundPresetRecall","Edit-3")); popup();
    require(poolButton->isChecked() && pool.content->window()->geometry()==updatedGeometry,
        "Update was not recalled or Dock all overwrote the saved preset");
    require(!main.findChild<QLabel *>("unboundPresetStatus")->text().contains("Recalled"),
        "Recall still shows redundant feedback");
    menu->hide(); pool.content->window()->move(340,270); settle();
    context(poolButton); menu=popup();
    require(control(menu,"unboundPresetUpdate","Update Edit-3"),"Modified preset was not dirty before switching away");
    click(control(menu,"unboundPresetRecall","Edit-2")); popup();
    require(!control(menu,"unboundPresetUpdate","Update Edit-3"),"Inactive preset retained a dirty update control");
    click(control(menu,"unboundPresetRecall","Edit-3")); popup();
    require(pool.content->window()->geometry()==updatedGeometry && !control(menu,"unboundPresetUpdate"),
        "Switching away retained an unsaved draft or changed the preset snapshot");
    const auto outside=menu->geometry().topLeft()-QPoint(20,20);
    QMouseEvent outsidePress(QEvent::MouseButtonPress,menu->mapFromGlobal(outside),outside,
        Qt::LeftButton,Qt::LeftButton,Qt::NoModifier);
    QApplication::sendEvent(menu,&outsidePress); settle();
    require(!menu->isVisible(),"Outside click did not dismiss the preset popup");
    menu->hide(); stack->setCurrentWidget(color); settle();
    const auto galleryButton=gallery.pane->findChild<QToolButton *>("resolvePaneUndockButton");
    context(galleryButton); menu=popup();
    require(!control(menu,"unboundPresetDockAll"),"Another page's floating panel enabled Dock all");
    click(control(menu,"unboundPresetSave")); popup();
    require(control(menu,"unboundPresetRecall","Color-1") && !control(menu,"unboundPresetRecall","Edit-2"),
        "Preset popup mixed page configurations");
    menu->hide(); click(galleryButton); settle();
    require(galleryButton->isChecked(),"Color Gallery failed to float for preset test");
    context(galleryButton); menu=popup(); click(control(menu,"unboundPresetSave")); popup();
    click(control(menu,"unboundPresetRecall","Color-1")); popup();
    require(!galleryButton->isChecked() && gallery.toggle->isChecked(),"Color preset did not dock Gallery with native visibility");
    click(control(menu,"unboundPresetRecall","Color-2")); popup();
    require(galleryButton->isChecked() && !gallery.toggle->isChecked() && gallery.content->isVisible(),
        "Color preset did not restore floating Gallery and native toggle state");
    menu->hide(); stack->setCurrentWidget(audio); settle();
    const auto groupsButton=groups.pane->findChild<QToolButton *>("resolvePaneUndockButton");
    context(groupsButton); menu=popup();
    click(control(menu,"unboundPresetSave")); popup();
    require(control(menu,"unboundPresetRecall","Fairlight-1"),"Fairlight header button did not expose presets");
    menu->hide(); click(groupsButton); settle();
    require(groupsButton->isChecked(),"Fairlight Groups failed to float for preset test");
    context(groupsButton); menu=popup(); click(control(menu,"unboundPresetSave")); popup();
    click(control(menu,"unboundPresetRecall","Fairlight-1")); popup();
    require(!groupsButton->isChecked() && groups.toggle->isChecked(),"Fairlight preset failed to dock Groups");
    click(control(menu,"unboundPresetRecall","Fairlight-2")); popup();
    require(groupsButton->isChecked() && !groups.toggle->isChecked() && groups.content->isVisible(),
        "Fairlight preset did not restore floating Groups and native toggle state");
    click(control(menu,"unboundPresetDockAll")); popup();
    require(!groupsButton->isChecked() && groups.toggle->isChecked(),"Fairlight Dock all did not restore Groups");
    menu->hide(); stack->setCurrentWidget(edit); settle();
    require(poolButton->isChecked(),"Fairlight Dock all affected Edit panels");
    context(poolButton); menu=popup();
    const auto outsidePoint=menu->geometry().bottomRight()+QPoint(20,20);
    QMouseEvent hover(QEvent::MouseMove,main.mapFromGlobal(outsidePoint),outsidePoint,
        Qt::NoButton,Qt::NoButton,Qt::NoModifier);
    QApplication::sendEvent(&main,&hover);
    require(menu->isVisible(),"Hovering outside dismissed the popout");
    const auto insidePoint=menu->mapToGlobal(menu->rect().center());
    QWheelEvent insideWheel(menu->mapFromGlobal(insidePoint),insidePoint,QPoint(),QPoint(0,120),
        Qt::NoButton,Qt::NoModifier,Qt::NoScrollPhase,false);
    QApplication::sendEvent(menu,&insideWheel);
    require(menu->isVisible(),"Scrolling inside dismissed the popout");
    auto dismissOutside=[&](QEvent &event) {
        context(poolButton); menu=popup(); QApplication::sendEvent(&main,&event);
        require(!menu->isVisible(),"Outside interaction failed to dismiss the popout");
    };
    QWheelEvent outsideWheel(main.mapFromGlobal(outsidePoint),outsidePoint,QPoint(),QPoint(0,120),
        Qt::NoButton,Qt::NoModifier,Qt::NoScrollPhase,false);
    dismissOutside(outsideWheel);
    for(auto type:{QEvent::MouseButtonPress,QEvent::MouseButtonRelease,QEvent::MouseButtonDblClick,
            QEvent::NonClientAreaMouseButtonPress,QEvent::NonClientAreaMouseButtonRelease,
            QEvent::NonClientAreaMouseButtonDblClick,QEvent::MouseMove}) {
        QMouseEvent mouse(type,main.mapFromGlobal(outsidePoint),outsidePoint,
            type==QEvent::MouseMove ? Qt::NoButton : Qt::LeftButton,Qt::LeftButton,Qt::NoModifier);
        dismissOutside(mouse);
    }
    QContextMenuEvent outsideContext(QContextMenuEvent::Mouse,main.mapFromGlobal(outsidePoint),outsidePoint);
    dismissOutside(outsideContext);
    QKeyEvent outsideKey(QEvent::KeyPress,Qt::Key_A,Qt::NoModifier,"a");
    dismissOutside(outsideKey);
    context(poolButton); menu=popup();
    QKeyEvent escape(QEvent::KeyPress,Qt::Key_Escape,Qt::NoModifier);
    QApplication::sendEvent(menu,&escape); settle();
    require(!menu->isVisible(),"Escape did not dismiss the persistent popout");
    context(poolButton); menu=popup();
    QEvent deactivate(QEvent::ApplicationDeactivate); QApplication::sendEvent(qApp,&deactivate); settle();
    require(!menu->isVisible(),"Application deactivation did not dismiss the persistent popout");
    std::puts("PASS: docked/floating context menus; persistent Save/Recall/Delete; retained hosts; docking, geometry and Edit/Color/Fairlight isolation");
    if(app.arguments().contains("--preview")) {
        main.move(main.pos()+QPoint(12,0)); settle();
        context(effects.pane->findChild<QToolButton *>("resolvePaneUndockButton"));
        if(app.arguments().contains("--preview-rename")) {
            auto preview=main.findChild<PagePresetMenu *>("unboundPresetMenu");
            hold(control(preview,"unboundPresetRecall","Edit-2"));
        }
        QTimer::singleShot(45000,&app,&QCoreApplication::quit); return app.exec();
    }
    QTimer::singleShot(0,&app,&QCoreApplication::quit);
    return app.exec();
}
