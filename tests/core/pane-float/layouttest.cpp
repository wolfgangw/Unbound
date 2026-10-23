// SPDX-License-Identifier: MIT
#include "pagelayoutmenu.h"
#include "panedecoration.h"
#include "controller.h"
#include <QApplication>
#include <QMenu>
#include <QMessageBox>
#include "layoutadapter.h"
#include "windowplacement.h"
#include <QTemporaryDir>
#include <QEventLoop>
#include <QStackedWidget>
#include <QPushButton>
#include <QStyleOptionButton>
#include <cstdio>
#include <cstdlib>
using namespace Unbound;
using LayoutTest::require;
using LayoutTest::settle;
// Resolve can swallow sendEvent(editor, keyPress) before application/object
// filters, while shortcut overrides and releases still pass.
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
static void click(QWidget *widget) {
    require(widget,"Missing layout menu control");
    // sendEvent does not perform the foreground activation of a real click.
    // Without it, docking the previously active floating host can transfer
    // focus to another application and correctly dismiss the popup.
    auto window=widget->window();
    if(qobject_cast<QMenu *>(window) && window->parentWidget()) window=window->parentWidget()->window();
    LayoutTest::activate(*window);
    const auto point=widget->rect().center();
    QMouseEvent press(QEvent::MouseButtonPress,point,widget->mapToGlobal(point),Qt::LeftButton,Qt::LeftButton,Qt::NoModifier);
    QMouseEvent release(QEvent::MouseButtonRelease,point,widget->mapToGlobal(point),Qt::LeftButton,Qt::NoButton,Qt::NoModifier);
    QApplication::sendEvent(widget,&press); QApplication::sendEvent(widget,&release); settle();
}
static void context(QWidget *button) {
    LayoutTest::activate(*button->window());
    const auto point=button->rect().center();
    QContextMenuEvent event(QContextMenuEvent::Mouse,point,button->mapToGlobal(point));
    QApplication::sendEvent(button,&event); settle();
}
static QAbstractButton *control(PageLayoutMenu *menu,const QString &object,const QString &text={}) {
    for(auto button:menu->findChildren<QAbstractButton *>(object))
        if(button->isVisible() && (text.isEmpty() || button->text()==text || button->accessibleName()==text)) return button;
    return nullptr;
}
static void layoutHighlights(QAbstractButton *active,QAbstractButton *inactive) {
    require(active && inactive && active->property("activeLayout").toBool()
        && !inactive->property("activeLayout").toBool(),"Layout recall did not transfer active styling");
    auto background=[](QAbstractButton *button,QStyle::State interaction) {
        QStyleOptionButton option; option.initFrom(button); option.rect=button->rect();
        option.state=QStyle::State_Enabled | QStyle::State_Active | QStyle::State_Raised | interaction;
        QImage image(button->size(),QImage::Format_ARGB32); image.fill(Qt::transparent);
        QPainter painter(&image);
        button->style()->drawControl(QStyle::CE_PushButton,&option,&painter,button);
        painter.end();
        return image.pixelColor(button->width()/2,2);
    };
    for(auto state:{QStyle::State_None,QStyle::State_MouseOver,QStyle::State_HasFocus}) {
        require(background(active,state)==QColor("#40566a"),"Active layout lost blue highlight on hover/focus");
        if(state!=QStyle::State_None)
            require(background(inactive,state)==QColor("#414141"),"Inactive layout lost grey hover/focus highlight");
    }
}
static void hold(QWidget *widget,bool activate=true) {
    require(widget,"Missing hold target");
    auto target=widget->window();
    if(!activate && target->parentWidget()) target=target->parentWidget()->window();
    LayoutTest::activate(*target);
    const auto point=widget->rect().center(),global=widget->mapToGlobal(point);
    QMouseEvent press(QEvent::MouseButtonPress,point,global,Qt::LeftButton,Qt::LeftButton,Qt::NoModifier);
    QApplication::sendEvent(widget,&press);
    QEventLoop loop; QTimer::singleShot(1100,&loop,&QEventLoop::quit); loop.exec();
    QMouseEvent release(QEvent::MouseButtonRelease,point,global,Qt::LeftButton,Qt::NoButton,Qt::NoModifier);
    QApplication::sendEvent(widget,&release); settle();
}
static QString layoutId(const PaneSettings &settings,const QString &page,const QString &name) {
    QString id;
    for(const auto &value:settings.layouts(page)) if(value.toObject().value("name").toString()==name) {
        require(id.isEmpty(),"Test expected a unique layout label"); id=value.toObject().value("id").toString();
    }
    require(!id.isEmpty(),"Test could not find the layout ID"); return id;
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
        "Layout migration changed existing layouts or invented layouts");
    QString name; bool duplicate=false;
    require(settings.saveLayout(edit,name,duplicate).isEmpty() && name=="Edit-1" && !duplicate,"First layout naming failed");
    require(settings.saveLayout(edit,name,duplicate).isEmpty() && duplicate && settings.layouts(edit).size()==1,
        "Identical page configuration consumed a layout slot");
    require(settings.saveLayout(color,name,duplicate).isEmpty() && name=="Color-1","Layout names were not page-local");
    const auto first=settings.layout(edit,layoutId(settings,edit,"Edit-1"));
    for(int i=2;i<=5;++i) {
        sample.move(70+i*20,90+i*15); settings.save("frameEditPageMediaPool",sample.saveGeometry());
        require(settings.saveLayout(edit,name,duplicate).isEmpty() && !duplicate,"Could not fill five layout slots");
    }
    sample.move(240,220); settings.save("frameEditPageMediaPool",sample.saveGeometry());
    require(!settings.saveLayout(edit,name,duplicate).isEmpty() && settings.layouts(edit).size()==5,"Layout limit was not enforced");
    require(settings.deleteLayout(edit,layoutId(settings,edit,"Edit-3")).isEmpty(),"Cannot delete layout");
    require(settings.saveLayout(edit,name,duplicate).isEmpty() && name=="Edit-6","Deletion reused a name or prevented saving");
    const auto colorBefore=settings.pageConfiguration(color);
    require(settings.activateLayout(edit,layoutId(settings,edit,"Edit-1")).isEmpty() && settings.pageConfiguration(edit)==first,
        "Layout recall did not restore the saved configuration");
    require(settings.pageConfiguration(color)==colorBefore && settings.layouts(edit).size()==5,
        "Layout recall modified another page or its archived layouts");
    PaneSettings reloaded(path);
    require(reloaded.layout(edit,layoutId(reloaded,edit,"Edit-1"))==first && reloaded.layouts(edit).size()==5 && reloaded.layouts(color).size()==1,
        "Saved layouts did not survive disk reload");
    require(reloaded.activeLayout(edit)==layoutId(reloaded,edit,"Edit-1") && !reloaded.layoutDirty(edit,layoutId(reloaded,edit,"Edit-1")),
        "Active layout did not persist or was dirty immediately after recall");
    reloaded.saveFloating("frameEditPageMediaPool",false);
    require(reloaded.activeLayout(edit)==layoutId(reloaded,edit,"Edit-1") && reloaded.layoutDirty(edit,layoutId(reloaded,edit,"Edit-1")),
        "Manual changes lost active identity or failed to mark it dirty");
    require(reloaded.updateLayout(edit,layoutId(reloaded,edit,"Edit-1")).isEmpty() && !reloaded.layoutDirty(edit,layoutId(reloaded,edit,"Edit-1")) &&
        reloaded.layouts(edit).size()==5 && reloaded.layout(edit,layoutId(reloaded,edit,"Edit-2"))==settings.layout(edit,layoutId(settings,edit,"Edit-2")),
        "Update did not replace only the selected layout in place");
    require(reloaded.deleteLayout(edit,layoutId(reloaded,edit,"Edit-1")).isEmpty() && reloaded.activeLayout(edit).isEmpty(),
        "Deleting the active layout retained its identity");
    QFile invalid(temporary.filePath("future.json")); require(invalid.open(QIODevice::WriteOnly),"Cannot seed future format");
    const QByteArray future="{\"version\":99,\"pages\":{}}"; invalid.write(future); invalid.close();
    PaneSettings unsupported(invalid.fileName());
    require(!unsupported.saveLayout(edit,name,duplicate).isEmpty(),"Future settings format was overwritten by layout save");
    require(invalid.open(QIODevice::ReadOnly) && invalid.readAll()==future,"Unsupported settings were modified"); invalid.close();
    QFile blocking(temporary.filePath("not-a-directory")); require(blocking.open(QIODevice::WriteOnly),"Cannot seed failed-write test"); blocking.close();
    PaneSettings unwritable(blocking.fileName()+"/layouts.json");
    require(!unwritable.saveLayout(edit,name,duplicate).isEmpty() && unwritable.layouts(edit).isEmpty(),
        "Failed layout write changed the in-memory layout list");
    std::puts("PASS: v5 migration, page-local naming, duplicate suppression, five-layout limit, delete/save, immutable recall and disk reload");
}
static void layoutIdentity() {
    QTemporaryDir directory; const auto path=directory.filePath("identity.json");
    const QString page="WStackPage_Conform";
    PaneSettings settings(path); QWidget sample;
    QStringList ids; QList<QJsonObject> configurations;
    for(int i=0;i<5;++i) {
        sample.setGeometry(50+i*20,70,400,300); settings.save("frameEditPageMediaPool",sample.saveGeometry());
        QString name; bool duplicate=false;
        require(settings.saveLayout(page,name,duplicate).isEmpty(),"Cannot create identity fixture");
        ids.append(settings.activeLayout(page)); configurations.append(settings.pageConfiguration(page));
    }
    const auto active=settings.activeLayout(page); const auto working=settings.pageConfiguration(page);
    for(const auto &id:ids) require(settings.renameLayout(page,id,"doodoo").isEmpty(),"Duplicate name was rejected");
    require(settings.activeLayout(page)==active && settings.pageConfiguration(page)==working && settings.layouts(page).size()==5,
        "Rename changed the active configuration or merged duplicate names");
    for(int i=0;i<ids.size();++i) require(settings.activateLayout(page,ids[i]).isEmpty() &&
        settings.pageConfiguration(page)==configurations[i],"Duplicate-name recall selected the wrong layout");
    settings.saveFloating("frameEditPageMediaPool",true);
    const auto untouched=settings.layout(page,ids[1]);
    require(settings.updateLayout(page,ids[0]).isEmpty() && settings.layout(page,ids[1])==untouched &&
        settings.layout(page,ids[0])!=configurations[0],"Duplicate-name update changed the wrong layout");
    require(!settings.renameLayout(page,ids[0],"   ").isEmpty() && !settings.renameLayout(page,ids[0],QString(129,'x')).isEmpty(),
        "Blank or overlong name accepted");
    require(settings.renameLayout(page,ids[0],"  & wow  ").isEmpty() && settings.layoutName(page,ids[0])=="& wow",
        "Name normalization failed");
    require(settings.deleteLayout(page,ids[2]).isEmpty() && settings.layouts(page).size()==4 &&
        settings.layout(page,ids[2]).isEmpty() && settings.layout(page,ids[1])==untouched,"Duplicate-name deletion targeted the wrong layout");
    PaneSettings reloaded(path);
    require(reloaded.layouts(page)==settings.layouts(page) && reloaded.activeLayout(page)==ids[0],"Layout IDs were not stable on reload");
    // Version 6 used names as identities. Migrate its active name and every
    // archived snapshot without changing the working layout or generated counter.
    QFile file(path); require(file.open(QIODevice::ReadOnly),"Cannot read identity fixture");
    auto root=QJsonDocument::fromJson(file.readAll()).object(); file.close();
    auto pages=root.value("pages").toObject(); auto state=pages.value(page).toObject();
    require(root.value("version").toInt()==7 && state.value("presets").toArray()==reloaded.layouts(page)
        && state.value("activePreset").toString()==reloaded.activeLayout(page)
        && state.value("nextPresetNumber").toInt()==6 && !state.contains("layouts") && !state.contains("activeLayout"),
        "Layout terminology changed the format-7 wire keys, identities or generated counter");
    auto saved=state.value("presets").toArray();
    for(int i=0;i<saved.size();++i) {
        auto item=saved[i].toObject(); item.remove("id"); item.insert("name",QString("Legacy-%1").arg(i)); saved[i]=item;
    }
    state.insert("presets",saved); state.insert("activePreset","Legacy-1"); pages.insert(page,state);
    root.insert("version",6); root.insert("pages",pages);
    require(file.open(QIODevice::WriteOnly|QIODevice::Truncate),"Cannot seed v6 fixture"); file.write(QJsonDocument(root).toJson()); file.close();
    PaneSettings migrated(path);
    require(migrated.readError().isEmpty() && migrated.pageConfiguration(page)==settings.pageConfiguration(page) &&
        migrated.activeLayout(page)==layoutId(migrated,page,"Legacy-1"),"v6 migration changed layout or lost active identity");
    for(int i=0;i<saved.size();++i) require(migrated.layout(page,layoutId(migrated,page,QString("Legacy-%1").arg(i)))==
        saved[i].toObject().value("configuration").toObject(),"v6 migration changed an archived snapshot");
    const auto migratedIds=migrated.layouts(page); PaneSettings migratedAgain(path);
    require(migratedAgain.layouts(page)==migratedIds,"Migration regenerated persisted IDs");
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
void testPaneCatalogue();
void testColorClips();
void testColorClipsStartup();
void testPageFocus();
void testIndexHeaderDrag();
void testDockingGuard();
int main(int argc,char **argv) {
    std::setvbuf(stdout,nullptr,_IONBF,0);
    LayoutTest::initialize();
    HostApplication app(argc,argv); if(app.arguments().contains("--page-focus-only")) { testPageFocus(); return 0; }
    if(app.arguments().contains("--color-clips-startup")) { testColorClipsStartup(); return 0; }
    if(app.arguments().contains("--index-header-drag")) { testIndexHeaderDrag(); return 0; }
    LayoutTest::nativePlacementEdges(); testPaneCatalogue(); testColorClips(); storage(); layoutIdentity();
    testPageFocus();
    testDockingGuard();
    QTemporaryDir directory;
    qputenv("UNBOUND_SETTINGS_FILE",directory.filePath("layouts.json").toUtf8());
    auto mainOwner=LayoutTest::makeMainWindow();
    auto &main=*mainOwner;
    auto theme=main.palette(); theme.setColor(QPalette::Window,QColor("#202024"));
    theme.setColor(QPalette::WindowText,QColor("#dddddd")); main.setPalette(theme);
    main.setObjectName("UiMainWindow"); main.setWindowTitle("Unbound layout fixture");
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
    const auto initialNative=LayoutTest::prepareMainPlacement(main);
    require(startController(),"Cannot start layout controller"); settle();
    auto poolButton=pool.pane->findChild<QToolButton *>("resolvePaneUndockButton");
    require(poolButton,"Controller did not decorate Media Pool");
    auto popup=[&]() {
        static int checkNumber=0; ++checkNumber;
        auto menu=main.findChild<PageLayoutMenu *>("unboundLayoutMenu");
        if(!menu || !menu->isVisible()) std::fprintf(stderr,"Popup check %d; menu exists=%d\n",checkNumber,menu!=nullptr);
        require(menu && menu->isVisible(),"Layout popup did not open or closed unexpectedly"); return menu;
    };
    context(poolButton); auto menu=popup();
    require(control(menu,"unboundLayoutSave")->text()=="Save as new Edit layout","New layout save label is incorrect");
    const auto anchor=poolButton->mapToGlobal(QPoint(0,poolButton->height()));
    require(menu->pos().y()==anchor.y() && !menu->geometry().intersects(QRect(poolButton->mapToGlobal(QPoint()),poolButton->size())),
        "Popup overlaps its button instead of starting directly below it");
    require(!control(menu,"unboundLayoutDockAll"),"Dock all appears with no floating panels");
    const auto background=menu->grab().toImage().pixelColor(2,menu->height()/2);
    require(background.alpha()==255 && background.rgb()==main.palette().color(QPalette::Window).rgb(),
        "Popout body is transparent or has the wrong theme background");
    const auto originalMainGeometry=main.geometry();
    require(poolButton->isChecked()==false,"Right-click toggled docking");
    click(control(menu,"unboundLayoutSave")); popup();
    require(control(menu,"unboundLayoutRecall","Edit-1"),"Save did not update the open menu");
    require(control(menu,"unboundLayoutRecall","Edit-1")->property("activeLayout").toBool() &&
        !control(menu,"unboundLayoutUpdate"),"Saved layout is not highlighted and clean");
    click(control(menu,"unboundLayoutSave")); popup();
    require(main.findChild<QLabel *>("unboundLayoutStatus")->text().contains("Already saved"),"Duplicate save was not reported");
    menu->hide(); click(poolButton); settle();
    require(poolButton->isChecked() && pool.content->window()!=&main,"Pool did not float");
    auto floating=pool.content->window(); floating->setGeometry(120,150,440,320); settle();
    const auto savedGeometry=floating->geometry();
    main.setGeometry(70,80,680,460); settle();
    const auto savedMainGeometry=main.geometry();
    context(poolButton); menu=popup();
    require(control(menu,"unboundLayoutDockAll"),"Dock all is unavailable with a floating panel");
    click(control(menu,"unboundLayoutSave")); popup();
    require(control(menu,"unboundLayoutRecall","Edit-2"),"Floating configuration was not saved");
    LayoutTest::verifyGestureDismissal(*floating,main,*menu,[&]() -> QWidget * { context(poolButton); menu=popup(); return menu; });
    PopupEvents popupEvents; menu->installEventFilter(&popupEvents);
    const auto realRecall=menu->recall; int appliedRecalls=0; bool internalDismissal=false;
    menu->recall=[&](const QString &name,bool &applied) {
        const auto error=realRecall(name,applied);
        if(applied) ++appliedRecalls;
        if(applied && internalDismissal) {
            QEvent deactivated(QEvent::WindowDeactivate);
            QApplication::sendEvent(menu,&deactivated);
            LayoutTest::transientPopup(*menu);
            require(menu->isVisible(),"Native popup cleanup dismissed the persistent layout popout");
        }
        return error;
    };
    auto inactive=control(menu,"unboundLayoutRecall","Edit-1");
    const auto inactiveId=inactive->property("layoutId").toString();
    WindowKeyInterceptor interceptor; interceptor.target=menu->windowHandle();
    qApp->installEventFilter(&interceptor);
    hold(inactive,false); popup();
    auto editor=menu->findChild<LayoutNameEdit *>("unboundLayoutNameEdit");
    for(auto candidate:menu->findChildren<LayoutNameEdit *>()) if(candidate->isVisible()) editor=candidate;
    require(editor && editor->isVisible() && appliedRecalls==0,"Long hold recalled instead of editing");
    app.blockedEditor=editor;
    require(editor->hasFocus() && QApplication::focusWidget()==editor && menu->isActiveWindow(),
        "Long hold selected text without activating keyboard input");
    require(LayoutTest::ownsKeyboardFocus(*menu),"Native keyboard focus stayed outside the name editor's window");
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
    require(!editor->isVisible() && control(menu,"unboundLayoutRecall","doodoo")==inactive &&
        control(menu,"unboundLayoutRecall","Edit-2")->property("activeLayout").toBool() && appliedRecalls==0 && poolButton->isChecked(),
        "Enter on an inactive layout renamed and recalled it");
    hold(inactive); editor->setText("cancelled"); QKeyEvent cancel(QEvent::KeyPress,Qt::Key_Escape,Qt::NoModifier);
    QApplication::sendEvent(editor,&cancel); settle(); popup();
    require(!editor->isVisible() && inactive->text()=="doodoo" && appliedRecalls==0,"Escape did not cancel rename without recall/dismissal");
    hold(inactive); editor->setText("Edit-1"); QApplication::sendEvent(editor,&submit); settle();
    require(inactive->property("layoutId").toString()==inactiveId && appliedRecalls==0,"Renaming replaced the layout identity");
    QPointer<QAbstractButton> retainedEdit1=control(menu,"unboundLayoutRecall","Edit-1");
    QPointer<QAbstractButton> retainedEdit2=control(menu,"unboundLayoutRecall","Edit-2");
    for(int repeat=0;repeat<3;++repeat) click(retainedEdit2);
    require(appliedRecalls==0 && retainedEdit2 && popupEvents.hides==0 && popupEvents.shows==0,
        "Repeated clean recall performed restoration or disturbed the popup");
    internalDismissal=true;
    floating->move(300,250); settle();
    main.setGeometry(90,100,640,420); settle();
    click(control(menu,"unboundLayoutRecall","Edit-2")); popup();
    internalDismissal=false;
    require(appliedRecalls==1,"Dirty active layout recall was incorrectly skipped");
    require(retainedEdit1 && retainedEdit2 && control(menu,"unboundLayoutRecall","Edit-2")==retainedEdit2,
        "Recall replaced the menu rows instead of updating them in place");
    require(pool.content->window()==floating && floating->geometry()==savedGeometry,
        "Recall rebuilt an already-floating panel or failed to restore its geometry");
    if(main.geometry()!=savedMainGeometry || pool.toggle->isChecked()) std::fprintf(stderr,
        "Main got %d,%d %dx%d expected %d,%d %dx%d toggle=%d\n",main.x(),main.y(),main.width(),main.height(),
        savedMainGeometry.x(),savedMainGeometry.y(),savedMainGeometry.width(),savedMainGeometry.height(),pool.toggle->isChecked());
    require(main.geometry()==savedMainGeometry && !pool.toggle->isChecked(),
        "Recall did not restore main-window geometry and the native pane toggle");
    click(control(menu,"unboundLayoutRecall","Edit-1")); popup();
    layoutHighlights(retainedEdit1,retainedEdit2);
    require(!poolButton->isChecked() && pool.content->window()==&main,"Recall did not dock the changed panel");
    require(retainedEdit1 && retainedEdit2,"Switching layouts destroyed retained menu rows");
    require(popupEvents.hides==0 && popupEvents.shows==0,"Switching layouts hid/reopened the popup");
    if(main.geometry()!=originalMainGeometry || !pool.toggle->isChecked()) std::fprintf(stderr,
        "Docked main got %d,%d %dx%d expected %d,%d %dx%d toggle=%d\n",main.x(),main.y(),main.width(),main.height(),
        originalMainGeometry.x(),originalMainGeometry.y(),originalMainGeometry.width(),originalMainGeometry.height(),pool.toggle->isChecked());
    require(main.geometry()==originalMainGeometry && pool.toggle->isChecked(),
        "Docked layout did not restore main geometry or native visibility");
    require(LayoutTest::nativeBounds(main)==initialNative,"Layout recall changed the native main-window bounds");
    click(control(menu,"unboundLayoutRecall","Edit-2")); popup();
    require(poolButton->isChecked() && pool.content->window()->geometry()==savedGeometry,"Recall did not restore floating geometry");
    layoutHighlights(retainedEdit2,retainedEdit1);
    click(control(menu,"unboundLayoutDelete","Delete Edit-1")); popup();
    require(!control(menu,"unboundLayoutRecall","Edit-1"),"Delete did not update the open menu");
    pool.content->window()->move(350,280); settle();
    click(control(menu,"unboundLayoutSave")); popup();
    require(control(menu,"unboundLayoutRecall","Edit-3"),"Delete-then-save sequence failed without dismissal");
    menu->hide(); pool.content->window()->move(320,260); settle();
    const auto updatedGeometry=pool.content->window()->geometry();
    context(poolButton); menu=popup();
    require(control(menu,"unboundLayoutRecall","Edit-3")->property("activeLayout").toBool() &&
        control(menu,"unboundLayoutUpdate","Update Edit-3"),"Dirty active layout has no update button");
    click(control(menu,"unboundLayoutUpdate","Update Edit-3")); popup();
    require(!control(menu,"unboundLayoutUpdate"),"Update left the layout dirty");
    click(control(menu,"unboundLayoutDockAll")); popup();
    require(!poolButton->isChecked() && pool.toggle->isChecked() &&
        !control(menu,"unboundLayoutRecall","Edit-3")->property("activeLayout").toBool(),
        "Dock all did not dock/show panels and clear active identity");
    require(!control(menu,"unboundLayoutDockAll"),"Dock all remained visible after docking every panel");
    click(control(menu,"unboundLayoutRecall","Edit-3")); popup();
    require(poolButton->isChecked() && pool.content->window()->geometry()==updatedGeometry,
        "Update was not recalled or Dock all overwrote the saved layout");
    require(!main.findChild<QLabel *>("unboundLayoutStatus")->text().contains("Recalled"),
        "Recall still shows redundant feedback");
    menu->hide(); pool.content->window()->move(340,270); settle();
    context(poolButton); menu=popup();
    require(control(menu,"unboundLayoutUpdate","Update Edit-3"),"Modified layout was not dirty before switching away");
    click(control(menu,"unboundLayoutRecall","Edit-2")); popup();
    require(!control(menu,"unboundLayoutUpdate","Update Edit-3"),"Inactive layout retained a dirty update control");
    click(control(menu,"unboundLayoutRecall","Edit-3")); popup();
    require(pool.content->window()->geometry()==updatedGeometry && !control(menu,"unboundLayoutUpdate"),
        "Switching away retained an unsaved draft or changed the layout snapshot");
    const auto outside=menu->geometry().topLeft()-QPoint(20,20);
    QMouseEvent outsidePress(QEvent::MouseButtonPress,menu->mapFromGlobal(outside),outside,
        Qt::LeftButton,Qt::LeftButton,Qt::NoModifier);
    QApplication::sendEvent(menu,&outsidePress); settle();
    require(!menu->isVisible(),"Outside click did not dismiss the layout popup");
    menu->hide(); stack->setCurrentWidget(color); settle();
    const auto galleryButton=gallery.pane->findChild<QToolButton *>("resolvePaneUndockButton");
    context(galleryButton); menu=popup();
    require(!control(menu,"unboundLayoutDockAll"),"Another page's floating panel enabled Dock all");
    click(control(menu,"unboundLayoutSave")); popup();
    require(control(menu,"unboundLayoutRecall","Color-1") && !control(menu,"unboundLayoutRecall","Edit-2"),
        "Layout popup mixed page configurations");
    menu->hide(); click(galleryButton); settle();
    require(galleryButton->isChecked(),"Color Gallery failed to float for layout test");
    context(galleryButton); menu=popup(); click(control(menu,"unboundLayoutSave")); popup();
    click(control(menu,"unboundLayoutRecall","Color-1")); popup();
    require(!galleryButton->isChecked() && gallery.toggle->isChecked(),"Color layout did not dock Gallery with native visibility");
    click(control(menu,"unboundLayoutRecall","Color-2")); popup();
    require(galleryButton->isChecked() && !gallery.toggle->isChecked() && gallery.content->isVisible(),
        "Color layout did not restore floating Gallery and native toggle state");
    menu->hide(); stack->setCurrentWidget(audio); settle();
    const auto groupsButton=groups.pane->findChild<QToolButton *>("resolvePaneUndockButton");
    context(groupsButton); menu=popup();
    click(control(menu,"unboundLayoutSave")); popup();
    require(control(menu,"unboundLayoutRecall","Fairlight-1"),"Fairlight header button did not expose layouts");
    menu->hide(); click(groupsButton); settle();
    require(groupsButton->isChecked(),"Fairlight Groups failed to float for layout test");
    context(groupsButton); menu=popup(); click(control(menu,"unboundLayoutSave")); popup();
    click(control(menu,"unboundLayoutRecall","Fairlight-1")); popup();
    require(!groupsButton->isChecked() && groups.toggle->isChecked(),"Fairlight layout failed to dock Groups");
    click(control(menu,"unboundLayoutRecall","Fairlight-2")); popup();
    require(groupsButton->isChecked() && !groups.toggle->isChecked() && groups.content->isVisible(),
        "Fairlight layout did not restore floating Groups and native toggle state");
    click(control(menu,"unboundLayoutDockAll")); popup();
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
    if(!app.arguments().contains("--preview")) {
        auto waitDock=[] { QEventLoop loop; QTimer::singleShot(180,&loop,&QEventLoop::quit); loop.exec(); };
        auto warning=dynamic_cast<UndockButton *>(poolButton);
        require(warning && poolButton->isChecked(),"Missing floating Media Pool before docking safeguard test");
        const auto floatingGeometry=pool.content->window()->geometry();
        // Model Resolve's checked, visible but one-pixel native destination.
        pool.content->window()->layout()->setSizeConstraint(QLayout::SetNoConstraint);
        pool.pane->setFixedWidth(1); pool.toggle->setChecked(true); pool.pane->show();
        click(poolButton); waitDock();
        require(poolButton->isChecked() && pool.content->window()!=&main && pool.content->isVisible(),
            "Failed docking lost the panel instead of floating it again");
        require(pool.content->window()->geometry()==floatingGeometry,"Failed docking lost floating geometry");
        PaneSettings checkedSettings(directory.filePath("layouts.json"));
        require(checkedSettings.floating("frameEditPageMediaPool"),"Failed docking persisted docked state");
        require(warning->dockingWarningPending(),"Failed docking did not start an unacknowledged warning");
        context(effects.pane->findChild<QToolButton *>("resolvePaneUndockButton")); popup()->hide();
        require(warning->dockingWarningPending(),"Another panel's menu acknowledged this warning");
        context(poolButton); menu=popup();
        auto explanation=menu->findChild<QLabel *>("unboundDockingWarning");
        require(explanation && explanation->isVisible() && explanation->text().contains("Use optimized UI layouts for vertical video"),
            "Media Pool menu lacks the remedy for collapsed docking");
        require(!warning->dockingWarningPending() && !warning->dockingWarningAnimating() && !warning->dockingFailure().isEmpty(),
            "Opening the affected menu did not stop the pulse while retaining the warning");
        menu->hide(); pool.content->window()->close(); waitDock();
        require(poolButton->isChecked() && warning->dockingWarningPending(),"Failed close did not restart the warning");
        context(poolButton); menu=popup(); click(control(menu,"unboundLayoutDockAll")); waitDock();
        require(poolButton->isChecked() && PaneSettings(directory.filePath("layouts.json")).floating("frameEditPageMediaPool"),
            "Dock all overrode the failed-docking recovery state");
        menu->hide();
        pool.pane->setMaximumWidth(QWIDGETSIZE_MAX); pool.pane->setMinimumWidth(0);
        click(poolButton); waitDock();
        require(!poolButton->isChecked() && pool.content->window()==&main && warning->dockingFailure().isEmpty(),
            "Successful docking did not clear the warning");
        context(poolButton); menu=popup();
        require(!menu->findChild<QLabel *>("unboundDockingWarning")->isVisible(),"Successful docking left a stale menu explanation");
        click(control(menu,"unboundLayoutSave"));
        PaneSettings savedDocked(directory.filePath("layouts.json"));
        const auto dockedId=savedDocked.activeLayout("WStackPage_Conform");
        const auto dockedSnapshot=savedDocked.layout("WStackPage_Conform",dockedId);
        menu->hide(); click(poolButton);
        pool.pane->setFixedWidth(1); pool.toggle->setChecked(true); pool.pane->show();
        context(poolButton); menu=popup();
        click(control(menu,"unboundLayoutRecall",savedDocked.layoutName("WStackPage_Conform",dockedId))); waitDock();
        PaneSettings failedRecall(directory.filePath("layouts.json"));
        require(poolButton->isChecked() && failedRecall.floating("frameEditPageMediaPool") &&
            failedRecall.activeLayout("WStackPage_Conform")==dockedId && failedRecall.layoutDirty("WStackPage_Conform",dockedId) &&
            failedRecall.layout("WStackPage_Conform",dockedId)==dockedSnapshot,
            "Failed layout docking overwrote its archive or misreported the actual floating state");
        require(control(menu,"unboundLayoutUpdate") && control(menu,"unboundLayoutDockAll"),
            "Open menu did not reflect recovered floating state and dirty layout");
        menu->hide(); pool.pane->setMaximumWidth(QWIDGETSIZE_MAX); pool.pane->setMinimumWidth(0);
        click(poolButton); waitDock();
        menu->hide();
        for(auto widget:QApplication::topLevelWidgets()) require(!qobject_cast<QMessageBox *>(widget),"Docking warning opened an intrusive message box");
        std::puts("PASS: failed docking/close/Dock all/layout recall retain geometry and saved floating state; panel-local pulse acknowledgment, remedy and recovery");
    }
    if(app.arguments().contains("--preview")) {
        main.move(main.pos()+QPoint(12,0)); settle();
        context(effects.pane->findChild<QToolButton *>("resolvePaneUndockButton"));
        if(app.arguments().contains("--preview-rename")) {
            auto preview=main.findChild<PageLayoutMenu *>("unboundLayoutMenu");
            hold(control(preview,"unboundLayoutRecall","Edit-2"));
        }
        QTimer::singleShot(45000,&app,&QCoreApplication::quit); return app.exec();
    }
    QTimer::singleShot(0,&app,&QCoreApplication::quit);
    return app.exec();
}
