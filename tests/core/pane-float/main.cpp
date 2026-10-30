#include "gridreferenceimage.h"
#include "panegridreferences.h"
#include "panegridsnap.h"
#include "fixtures.h"
#include "fairlighttest.h"

int main(int argc, char **argv)
{
    initializePlatformTests();
    QApplication app(argc, argv);
    if (runFairlightTest(app)) return 0;
    if (app.arguments().contains("--eliding-label-test")) {
        const QVector<Unbound::PaneCatalogue::ElidingLabelDescriptor> cases={
            {"frameEditPageInspector","frameEffectsWidgetsTitle","labelTitle"},
            {"frameInspectorContainer","frameEffectsWidgetsTitle","labelTitle"},
            {"frameEditPageMetadata","frameClipInfoTop","lblMetadataFileName"},
            {"frameEditPageMetadata","framePhotoInfoTop","lblMetadataPhotoFileName"}};
        for(const auto &spec:cases) {
            QWidget page; page.resize(700,500);
            auto slot=new QWidget(&page); slot->setObjectName(spec.pane); slot->resize(600,400);
            auto outer=new QVBoxLayout(slot);
            auto bodyContainer=new QWidget(slot); outer->addWidget(bodyContainer);
            auto column=new QVBoxLayout(bodyContainer);
            auto header=new QWidget(bodyContainer); header->setObjectName(spec.scope); column->addWidget(header);
            auto row=new QHBoxLayout(header);
            auto action=new QPushButton("Menu",header); action->setFixedSize(40,24); row->addWidget(action);
            const QString filename=QString(180,QLatin1Char('W'))+QStringLiteral("_[000001-999999].dpx");
            auto title=new ElidingTitleLabel(filename,header); title->setObjectName(spec.label);
            title->setProperty("elideMode",int(Qt::ElideMiddle));
            QSizePolicy original(QSizePolicy::Expanding,QSizePolicy::Preferred);
            original.setHorizontalStretch(2); title->setSizePolicy(original); row->addWidget(title);
            auto content=new QWidget(bodyContainer); content->setMinimumSize(360,100); column->addWidget(content);
            page.show(); app.processEvents();
            const auto fullWidth=title->minimumSizeHint().width();
            check(fullWidth>1000,"Pane fixture filename is too short");
            auto unrelated=new ElidingTitleLabel(filename,bodyContainer);
            unrelated->setObjectName(spec.label); unrelated->setProperty("elideMode",int(Qt::ElideMiddle));
            unrelated->setSizePolicy(original); unrelated->hide();
            const auto unrelatedPolicy=unrelated->sizePolicy();
            QPointer<ElidingTitleLabel> late;
            QSizePolicy lateOriginal(QSizePolicy::Expanding,QSizePolicy::Preferred);
            for (int pass=0;pass<4;++pass) {
                original.setHorizontalStretch(pass%2 ? 2 : 0);
                title->setSizePolicy(original);
                Unbound::SidebarFloatSession session;
                check(session.start(slot,spec.pane,&page).isEmpty(),"Pane detach failed");
                auto host=session.floatingWindow(); host->resize(420,300); app.processEvents();
                check(host->width()==420 && host->minimumWidth()>=360 && host->minimumWidth()<fullWidth,
                    qPrintable(QString("Filename still constrains floating width or content minimum was lost: width=%1 minimum=%2 full=%3").arg(host->width()).arg(host->minimumWidth()).arg(fullWidth)));
                check(title->width()<fullWidth && title->text()==filename && title->property("elideMode").toInt()==int(Qt::ElideMiddle),
                    "Native label lost its full text/elision mode or cannot shrink");
                check(unrelated->sizePolicy()==unrelatedPolicy,"Label outside catalogued scope was changed");
                if(pass==0) {
                    auto lateScope=new QWidget(bodyContainer); lateScope->setObjectName(spec.scope);
                    auto lateRow=new QHBoxLayout(lateScope); column->addWidget(lateScope);
                    late=new ElidingTitleLabel(filename,lateScope); late->setObjectName(spec.label);
                    late->setProperty("elideMode",int(Qt::ElideMiddle));late->setSizePolicy(lateOriginal);
                    lateRow->addWidget(late);lateScope->show();app.processEvents();
                    check(late->sizePolicy().horizontalPolicy()==QSizePolicy::Ignored,
                        "Late/repeated native title variant retained its full-text hint");
                }
                host->resize(420,300); app.processEvents();
                const int narrowTitleWidth=title->width();
                host->resize(520,300); app.processEvents();
                check(title->width()>narrowTitleWidth,"Title does not use additional header space");
                host->resize(420,300); app.processEvents();
                check(title->sizePolicy().horizontalStretch()==qMax(1,original.horizontalStretch())
                    && title->sizePolicy().verticalPolicy()==original.verticalPolicy(),"Title expansion/stretch/height policy changed");
                title->setText(filename+filename); app.processEvents();
                check(host->width()==420 && host->minimumWidth()<fullWidth,"Clip selection update widened the floating pane");
                title->setText(filename); app.processEvents();
                if (pass<2) {
                    check(session.dock().isEmpty(),"Pane docking failed");
                    check(title->sizePolicy()==original,"Docking did not restore title policy");
                }
                if(pass<2) check(late->sizePolicy()==lateOriginal,"Repeated title policy was not restored");
                // Last two passes exercise destructor restoration.
            }
            check(title->sizePolicy()==original && slot->layout()==outer,"Docking did not restore original policy/layout");
            title->setProperty("elideMode",int(Qt::ElideRight));
            {
                Unbound::SidebarFloatSession session;
                check(session.start(slot,spec.pane,&page).isEmpty(),"Unrecognized title detach failed");
                check(title->sizePolicy()==original,"Nonmatching title sizing was changed");
                auto newer=lateOriginal; newer.setHorizontalPolicy(QSizePolicy::MinimumExpanding);
                late->setSizePolicy(newer);
                check(session.dock().isEmpty(),"Final title fixture docking failed");
                check(late->sizePolicy()==newer,"Docking overwrote a newer native sizing policy");
            }
        }
        std::puts("PASS: Catalogued Inspector/Metadata titles shrink; native text/elision, late variants, content minimums and docking policy preserved");
        return 0;
    }
    if (runPlatformDiscoveryTests(app)) return 0;
    if (app.arguments().contains("--reclaim-test")) {
        WarningCapture warnings;
        app.installEventFilter(&warnings);
        QTemporaryDir directory;
        check(directory.isValid(), "Cannot create reclamation settings directory");
        const auto settingsPath = directory.filePath("Unbound.json");
        qputenv("UNBOUND_SETTINGS_FILE", settingsPath.toUtf8());
        QWidget main;
        main.setObjectName("UiMainWindow");
        auto edit = new QWidget(&main);
        edit->setObjectName("frameEditPageWidgetsContainer");
        auto color = new QWidget(&main);
        color->setObjectName("WStackPage_Color");
        const QStringList names = {"frameEditPageMediaPool", "frameEditPageEffectsLibrary",
            "frameEditPageSoundLibrary", "frameEditPageKeyframeEditor", "frameEditPageEditIndex",
            "frameEditPageInspector", "frameEditPageMetadata", "frameEditAudioMeter",
            "m_pFrameNodeGraph", "m_pWidgetGallery", "m_pWidgetLutBrowser", "m_pWidgetMediaPool", "m_pFrameOFXPlugin", "timelineFrame"};
        const QStringList toggles = {"btnEditToggleMediaPoolInSSM", "btnEditToggleEffectsLibraryInSSM",
            "btnEditToggleSoundLibraryInSSM", "btnEditToggleKeyframeEditorInSSM", "btnEditToggleEditIndexInSSM",
            "btnEditToggleInspectorInSSM", "btnEditToggleMetadataInSSM", "btnEditToggleMixerInSSM",
            "btnColorToggleNodeGraph", "btnColorToggleGallery", "btnColorToggleLutsBrowser", "btnColorToggleMediaPool", "btnColorToggleOpenFX", "btnColorToggleTimeline"};
        QVector<QWidget *> paneSlots;
        QVector<QPushButton *> controls, selectors;
        QVector<QLayout *> layouts;
        QVector<int> clicks(names.size(), 0), edits(names.size(), 0);
        for (int i = 0; i < names.size(); ++i) {
            auto page = i < 8 ? edit : color;
            QWidget *parent = page;
            if (i == 7) {
                auto splitter = new QSplitter(page);
                new QWidget(splitter);
                parent = splitter;
            }
            auto slot = new QWidget(parent); slot->setObjectName(names[i]);
            auto layout = new QVBoxLayout(slot);
            auto control = new QPushButton("Live content", slot); layout->addWidget(control);
            if(names[i]=="timelineFrame") {
                control->setObjectName("m_pTimeLinePnl"); slot->setFixedHeight(76);
                // Model the native canvas, not a fixed-height push-button row.
                control->setSizePolicy(QSizePolicy::Expanding,QSizePolicy::Expanding);
            }
            auto selector = new QPushButton("Localized native label", page);
            selector->setObjectName(toggles[i]); selector->setCheckable(true); selector->setChecked(true);
            paneSlots.append(slot); layouts.append(layout); controls.append(control); selectors.append(selector);
            QObject::connect(control, &QPushButton::clicked, page, [&, i] { ++edits[i]; });
            QObject::connect(selector, &QPushButton::clicked, page, [&, i, page](bool checked) {
                ++clicks[i];
                QTimer::singleShot(0, page, [&, i, page, checked] {
                    if (i >= 8) {
                        if (checked) layouts[i]->addWidget(controls[i]);
                        else controls[i]->setParent(page);
                        controls[i]->setVisible(checked);
                    }
                    else controls[i]->setVisible(checked);
                    paneSlots[i]->setVisible(checked);
                });
            });
        }
        auto alternate = new QLabel("Hidden alternate view", paneSlots[0]);
        layouts[0]->addWidget(alternate); alternate->hide();
        auto decoy = new QPushButton("Localized native label", edit);
        decoy->setCheckable(true); decoy->setChecked(true);
        auto hiddenVariant = new QPushButton(edit);
        hiddenVariant->setObjectName("btnEditToggleMediaPoolInDSM");
        hiddenVariant->setCheckable(true); hiddenVariant->setChecked(true); hiddenVariant->hide();
        main.show(); edit->show(); color->hide(); app.processEvents();
        QLibrary helper(app.arguments().last()); helper.setLoadHints(QLibrary::PreventUnloadHint);
        check(helper.load(), qPrintable(helper.errorString()));
        auto start = resolveBootstrap(helper);
        check(start && start(nullptr) == 1, "Reclamation helper bootstrap failed");
        app.processEvents();
        // An unsupported/empty layout must leave native visibility untouched.
        layouts[3]->removeWidget(controls[3]);
        auto failedButton = paneSlots[3]->findChild<QToolButton *>("resolvePaneUndockButton");
        check(failedButton, "Missing failed-detach button");
        failedButton->click(); app.processEvents();
        check(warnings.messages.size() == 1 && warnings.messages.first().contains("nonempty box or grid layout"),
            "Failed detach did not report its unsupported layout");
        check(selectors[3]->isChecked() && clicks[3] == 0 && controls[3]->parentWidget() == paneSlots[3],
            "Failed detach reclaimed space or moved content");
        layouts[3]->addWidget(controls[3]);
        for (int i = 0; i < names.size(); ++i) {
            if (i == 8) { edit->hide(); color->show(); app.processEvents(); }
            auto button = paneSlots[i]->findChild<QToolButton *>("resolvePaneUndockButton");
            check(button, "Missing reclamation button");
            button->click(); app.processEvents(); app.processEvents();
            auto host = controls[i]->window();
            check(host != &main && host->isVisible() && controls[i]->isVisible(), "Reclamation hid floating content");
            check(alternate->isHidden(), "Reclamation exposed explicitly hidden alternate content");
            check(!selectors[i]->isChecked() && paneSlots[i]->isHidden() && clicks[i] == 1,
                "Manual undock did not collapse native slot exactly once");
            if(names[i]=="timelineFrame") {
                auto header=host->findChild<QWidget *>("unboundColorTimelineHeader");
                check(header && button->parentWidget()==header && header->isVisible(),
                    "Floating Timeline button did not move to its own header");
                check(header->geometry().bottom()<controls[i]->geometry().top(),
                    "Floating Timeline header covers native playhead controls");
                check(header->findChild<QLabel *>()->text()=="Timeline", "Timeline header title missing");
                for(const QPoint point:{QPoint(100,5),QPoint(5,50)}) {
                    QMouseEvent press(QEvent::MouseButtonPress,point,controls[i]->mapToGlobal(point),
                        Qt::LeftButton,Qt::LeftButton,Qt::NoModifier);
                    QApplication::sendEvent(controls[i],&press);
                    QMouseEvent release(QEvent::MouseButtonRelease,point,controls[i]->mapToGlobal(point),
                        Qt::LeftButton,Qt::NoButton,Qt::NoModifier);
                    QApplication::sendEvent(controls[i],&release);
                }
                check(edits[i]==2,"Floating Timeline header intercepted native ruler/gutter input");
                check(paneSlots[i]->height()==76,"Timeline decoration changed native slot height");
                const int nativeMinimum=paneSlots[i]->minimumWidth();
                host->resize(1,400); app.processEvents();
                check(host->width()>=240 && host->height()==400,"Floating Timeline lacks usable minimum width or limits height");
                check(paneSlots[i]->minimumWidth()==nativeMinimum,"Floating Timeline minimum leaked into native slot");
                // Native width policy can turn the toggle off without a click.
                selectors[i]->setChecked(true); selectors[i]->setChecked(false);
                controls[i]->hide(); app.processEvents(); app.processEvents();
                check(controls[i]->isVisible() && controls[i]->window()==host,"Native automatic hiding swallowed floating Timeline");
            }
            const auto key = i < 8 ? names[i] : "Color/" + names[i];
            Unbound::PaneSettings saved(settingsPath);
            check(saved.floating(key) && saved.hasToggle(key) && !saved.toggle(key), "Reclaimed state not persisted");
            controls[i]->click(); check(edits[i] == (names[i]=="timelineFrame" ? 3 : 1), "Reclamation lost native signals");
            check(decoy->isChecked() && hiddenVariant->isChecked(), "Reclamation clicked an unrelated/hidden toggle");
            if (i == 0) {
                edit->hide(); app.processEvents();
                check(host->close(), "Hidden-page docking failed"); app.processEvents();
                check(!selectors[i]->isChecked() && !edit->isVisible(), "Hidden-page docking changed page/toggle prematurely");
                edit->show();
                QEventLoop ready; QTimer::singleShot(1100, &ready, &QEventLoop::quit); ready.exec();
            } else if (i % 2) button->click();
            else check(host->close(), "Reclamation docking rejected close");
            app.processEvents(); app.processEvents();
            check(selectors[i]->isChecked() && paneSlots[i]->isVisible() && controls[i]->parentWidget() == paneSlots[i]
                && controls[i]->isVisible() && paneSlots[i]->layout() == layouts[i], "Docking did not show restored content");
            if(names[i]=="timelineFrame") {
                check(button->parentWidget()==controls[i] && button->pos()==QPoint(6,2),
                    "Docking did not restore the Timeline corner button");
                check(!paneSlots[i]->findChild<QWidget *>("unboundColorTimelineHeader"),
                    "Floating header leaked into the docked Timeline");
            }
            check(clicks[i] == 2 && !Unbound::PaneSettings(settingsPath).floating(key)
                && Unbound::PaneSettings(settingsPath).toggle(key), "Docking intent/toggle not persisted");
        }
        // Model native/application close interception after restoring a pane.
        // A Dock command must restore the slot even if close() would only hide
        // the floating host and never reach the session's event filter.
        color->hide(); edit->show(); app.processEvents();
        auto poolButton = paneSlots[0]->findChild<QToolButton *>("resolvePaneUndockButton");
        poolButton->click(); app.processEvents(); app.processEvents();
        HostCloseInterceptor interceptor;
        interceptor.host = controls[0]->window();
        app.installEventFilter(&interceptor);
        poolButton->click(); app.processEvents(); app.processEvents();
        check(interceptor.closeEvents == 0 && selectors[0]->isChecked() && paneSlots[0]->isVisible()
            && controls[0]->parentWidget() == paneSlots[0] && controls[0]->isVisible()
            && paneSlots[0]->findChildren<QLabel *>("unboundSidebarPlaceholder").isEmpty()
            && !Unbound::PaneSettings(settingsPath).floating(names[0]),
            "Dock command depended on intercepted Close event or left placeholder/floating intent");
        app.removeEventFilter(&interceptor);
        check(warnings.messages.size() == 1, "Unexpected warning during reclamation/docking");
        QTimer::singleShot(0, &app, &QCoreApplication::quit); app.exec();
        std::puts("PASS: all Edit/Color panes reclaim on manual undock and show on docking; live content, localized labels, hidden variants, failed detach and hidden-page docking");
        return 0;
    }
    if (app.arguments().contains("--header-button-test")) {
        QWidget page; auto pane=new QWidget(&page); pane->setObjectName("frameEditPageMediaPool");
        auto content=new QVBoxLayout(pane); auto header=new QWidget(pane); header->setObjectName("frameMediaPoolTitleBarTop");
        auto row=new QHBoxLayout(header); row->setContentsMargins(0,0,0,0);
        auto label=new QLabel("Media Pool",header); row->addWidget(label); row->addStretch();
        auto menu=new QToolButton(header); menu->setObjectName("btnMediaPoolOptions"); menu->setText("..."); row->addWidget(menu);
        content->addWidget(header); content->addWidget(new QLabel("Content",pane));
        page.resize(600,400); pane->resize(500,300); page.show(); app.processEvents();
        const auto margins=pane->contentsMargins(); const int count=row->count(); int menuClicks=0;
        QObject::connect(menu,&QToolButton::clicked,[&]{++menuClicks;});
        Unbound::SidebarFloatSession session;
        {
            Unbound::PaneDecoration decoration(pane,nullptr,[&]{
                if(session.active()) check(session.floatingWindow()->close(),"Native-header toggle docking failed");
                else check(session.start(pane,"Media Pool",&page).isEmpty(),"Native-header detach failed");
            });
            app.processEvents(); auto button=decoration.button();
            check(decoration.usesNativeHeader() && pane->contentsMargins()==margins,"Native header retained extra strip");
            check(button && button->text().isEmpty() && !button->icon().isNull() && !button->icon().pixmap(20,20).isNull()
                && button->accessibleName()=="Undock" && button->toolTip()=="Undock" && button->isCheckable() && !button->isChecked(),"Undock symbol/accessibility missing");
            check(row->indexOf(button)+1==row->indexOf(menu) && row->count()==count+1,"Symbol is not immediately before native menu");
            menu->click(); check(menuClicks==1,"Native menu signal broken");
            button->click(); decoration.setFloating(true); app.processEvents();
            check(button->isVisible() && button->isChecked() && button->toolTip()=="Click to dock; drag to move" && header->window()==session.floatingWindow(),"Floating toggle state or tooltip incorrect");
            const auto off=button->icon().pixmap(QSize(20,20),QIcon::Normal,QIcon::Off).toImage();
            const auto on=button->icon().pixmap(QSize(20,20),QIcon::Normal,QIcon::On).toImage();
            check(on==off,"Floating state changed icon brightness");
            const auto hover=button->icon().pixmap(QSize(20,20),QIcon::Active,QIcon::On).toImage();
            check(hover!=off,"Hover did not brighten icon");
            for(int y=0;y<off.height();++y) for(int x=0;x<off.width();++x) {
                check(qAlpha(hover.pixel(x,y))==qAlpha(off.pixel(x,y)),"Hover changed stroke coverage/background");
                if(qAlpha(off.pixel(x,y))==255)
                    check(qRed(off.pixel(x,y))==132 && qRed(hover.pixel(x,y))==220,"Wrong icon stroke tones");
            }
            // Render the actual button with hover and checked states, ensuring
            // inherited Qt styles cannot paint a filled hover background.
            auto rendered=[&](bool hovered,bool checked) {
                button->setAttribute(Qt::WA_UnderMouse,hovered); button->setChecked(checked);
                QImage result(button->size(),QImage::Format_ARGB32_Premultiplied); result.fill(Qt::transparent);
                button->render(&result,QPoint(),QRegion(),QWidget::DrawChildren); return result;
            };
            const auto normalButton=rendered(false,false), floatingButton=rendered(false,true), hoverButton=rendered(true,true);
            check(normalButton==floatingButton && normalButton!=hoverButton,"Button hover/toggle rendering incorrect");
            for(int y=0;y<normalButton.height();++y) for(int x=0;x<normalButton.width();++x)
                check(qAlpha(normalButton.pixel(x,y))==qAlpha(hoverButton.pixel(x,y)),"Button hover painted outside strokes");
            button->setAttribute(Qt::WA_UnderMouse,false);
            button->click(); decoration.setFloating(false); app.processEvents();
            check(!session.active() && button->isVisible() && !button->isChecked() && button->toolTip()=="Undock"
                && row->indexOf(button)+1==row->indexOf(menu),"Native header button not restored on docking");
        }
        check(row->count()==count && pane->contentsMargins()==margins,"Native header cleanup changed original layout");
        QWidget lazy(&page); lazy.setObjectName("frameEditPageInspector"); lazy.show();
        {
            Unbound::PaneDecoration decoration(&lazy,nullptr,[]{});
            check(!decoration.usesNativeHeader(),"Unknown header should retain fallback");
            auto header=new QWidget(&lazy); header->setObjectName("frameEffectsWidgetsTitle");
            auto layout=new QHBoxLayout(header); auto menu=new QToolButton(header); menu->setObjectName("btnTitleOptions"); layout->addWidget(menu);
            decoration.setFloating(false); app.processEvents();
            check(decoration.usesNativeHeader() && lazy.contentsMargins()==QMargins(),"Late native header did not replace fallback strip");
        }
        {
            QWidget color; color.setObjectName("WStackPage_Color");
            QWidget slot(&color); slot.setObjectName("m_pFrameOFXPlugin");
            auto grid=new QGridLayout(&slot);
            auto content=new QWidget(&slot); auto body=new QVBoxLayout(content); grid->addWidget(content,0,0);
            auto title=new QWidget(content); title->setObjectName("frameOFXTitle"); body->addWidget(title);
            auto right=new QWidget(title); right->setObjectName("frameTopRight");
            auto titleRow=new QHBoxLayout(title); titleRow->addWidget(right);
            auto row=new QHBoxLayout(right);
            auto menu=new QToolButton(right); menu->setObjectName("btnOFXPoolOptions"); row->addWidget(menu);
            color.resize(600,400); slot.resize(400,300); color.show(); slot.show(); app.processEvents();
            Unbound::SidebarFloatSession effects;
            Unbound::PaneDecoration decoration(&slot,nullptr,[&] {
                if(effects.active()) check(effects.dock().isEmpty(),"Color Effects docking failed");
                else check(effects.start(&slot,"Effects",&color,false,false,true).isEmpty(),"Color Effects detach failed");
            });
            auto button=decoration.button();
            check(decoration.usesNativeHeader() && row->indexOf(button)+1==row->indexOf(menu),"Color Effects native anchor not recognized");
            button->click(); app.processEvents();
            check(content->window()==effects.floatingWindow() && slot.layout()==grid,"Color Effects must retain native grid and float whole inspector");
            button->click(); app.processEvents();
            check(content->parentWidget()==&slot && grid->indexOf(content)==0,"Color Effects content not restored to native slot");
        }
        std::puts("PASS: native header icon placement, Color Effects grid/content lifecycle, menu signals, detach/dock, cleanup and lazy-header fallback upgrade");
        return 0;
    }
    if (app.arguments().contains("--settings-json-test")) {
        QTemporaryDir dir;
        const auto oldOverride=qgetenv("UNBOUND_SETTINGS_FILE");
        qunsetenv("UNBOUND_SETTINGS_FILE");
        QDir().mkpath(dir.filePath("Unbound"));
        QFile oldFile(dir.filePath("Unbound/settings.json"));
        check(oldFile.open(QIODevice::WriteOnly),"Cannot create version 4 settings");
        oldFile.write(R"({"version":4,"pages":{"color":{"panes":{"luts":{"floating":true,"toolbarButtonChecked":false}}},"edit":{"panes":{"mediaPool":{"floating":true}}}}})"); oldFile.close();
        Unbound::PaneSettings renamed({},dir.path());
        check(renamed.floating("Color/m_pWidgetLutBrowser") && renamed.hasToggle("Color/m_pWidgetLutBrowser")
            && !renamed.toggle("Color/m_pWidgetLutBrowser") && renamed.floating("frameEditPageMediaPool"),"Version 4 migration lost settings");
        check(!oldFile.exists(),"Old filename retained after migration");
        QFile newFile(dir.filePath("Unbound/Unbound.json"));
        check(newFile.open(QIODevice::ReadOnly),"New settings filename missing");
        const auto migrated=newFile.readAll(); newFile.close();
        check(migrated.contains("WStackPage_Color") && migrated.contains("m_pWidgetLutBrowser")
            && !migrated.contains("\"luts\""),"Migration retained friendly widget aliases");
        if(oldOverride.isNull()) qunsetenv("UNBOUND_SETTINGS_FILE"); else qputenv("UNBOUND_SETTINGS_FILE",oldOverride);
        const auto path=dir.filePath("Unbound.json");
        QWidget window; window.setGeometry(100,120,434,600); window.show(); app.processEvents();
        const auto original=window.saveGeometry();
        QFile legacy(path); check(legacy.open(QIODevice::WriteOnly),"Cannot create migration input");
        legacy.write(QJsonDocument(QJsonObject{{"version",3},
            {"panes",QJsonObject{{"frameEditPageMediaPool",QString::fromLatin1(original.toBase64())},
                {"Color/m_pLightBox",QString::fromLatin1(original.toBase64())}}},
            {"floating",QJsonObject{{"frameEditPageMediaPool",true},{"Color/m_pLightBox",true}}},
            {"toggles",QJsonObject{{"frameEditPageMediaPool",false},{"Color/m_pLightBox",false}}}}).toJson()); legacy.close();
        Unbound::PaneSettings settings(path);
        check(settings.geometry("frameEditPageMediaPool")==original,"Geometry changed during readable conversion");
        check(settings.floating("frameEditPageMediaPool") && settings.hasToggle("frameEditPageMediaPool")
            && !settings.toggle("frameEditPageMediaPool"),"Conversion lost intent");
        QFile file(path); check(file.open(QIODevice::ReadOnly),"Cannot read converted file");
        const auto bytes=file.readAll(); file.close();
        check(!bytes.contains("m_pLightBox") && !bytes.contains(original.toBase64()),"Obsolete pane or encoded geometry retained");
        auto root=QJsonDocument::fromJson(bytes).object();
        check(root.value("version").toInt()==7,"Readable format version missing");
        auto pages=root.value("pages").toObject(); auto edit=pages.value("WStackPage_Conform").toObject();
        auto panes=edit.value("panes").toObject(); auto pool=panes.value("frameEditPageMediaPool").toObject();
        auto geometry=pool.value("window").toObject(); auto client=geometry.value("clientRect").toObject();
        client.insert("width",510); geometry.insert("clientRect",client); pool.insert("window",geometry);
        panes.insert("frameEditPageMediaPool",pool); edit.insert("panes",panes); pages.insert("WStackPage_Conform",edit); root.insert("pages",pages);
        check(file.open(QIODevice::WriteOnly),"Cannot simulate user JSON edit"); file.write(QJsonDocument(root).toJson()); file.close();
        Unbound::PaneSettings edited(path); QWidget restored;
        check(restored.restoreGeometry(edited.geometry("frameEditPageMediaPool")) && restored.width()==510,"User width edit not honored");
        client.insert("width",-10); geometry.insert("clientRect",client);
        check(Unbound::qtGeometryBytes(geometry).isEmpty(),"Invalid user rectangle accepted");
        const QJsonObject native{{"format","unbound-main-placement-1"},{"normal",QJsonArray{50,60,650,460}},
            {"maximized",false},{"screens",QJsonArray{QJsonValue(QJsonArray{"DISPLAY1",0,0,1920,1040,1.25})}},
            {"qt",QString::fromLatin1(original.toBase64())}};
        check(settings.save("MainWindow/WStackPage_Conform",QJsonDocument(native).toJson()).isEmpty(),"Cannot store native placement");
        check(file.open(QIODevice::ReadOnly),"Cannot read main placement"); const auto mainBytes=file.readAll(); file.close();
        check(mainBytes.contains("DISPLAY1") && mainBytes.contains("workArea") && mainBytes.contains("scaleFactor")
            && !mainBytes.contains(original.toBase64()),"Monitor or fallback data is encoded");
        check(QJsonDocument::fromJson(settings.geometry("MainWindow/WStackPage_Conform")).object()==native,"Native placement conversion changed data");
        std::puts("PASS: readable settings migration, obsolete Clips removal, direct geometry edits, validation and native monitor roundtrip");
        return 0;
    }
    if (app.arguments().contains("--nodes-canvas-test")) {
        QWidget host;
        auto layout=new QVBoxLayout(&host);
        auto view=new ResizeObservedGraph(&host); view->setObjectName("m_NodeGraphWgt"); layout->addWidget(view);
        QGraphicsScene scene;
        scene.setSceneRect(-1250,-1250,2500,2500); view->setScene(&scene); view->scale(.6,.6);
        auto node=scene.addRect(-30,-20,60,40); node->setPos(-1490,-69);
        const auto position=node->pos();
        host.resize(3200,350); host.show(); app.processEvents();
        Unbound::NodesCanvasBounds bounds(&host);
        const auto resizeCalls=view->resizeCalls;
        bounds.updateBounds();
        check(view->resizeCalls>resizeCalls && view->sceneAtResize==scene.sceneRect(),"Graph resize handler did not receive expanded bounds");
        check(scene.sceneRect().contains(view->mapToScene(view->viewport()->rect()).boundingRect()),"Expanded scene does not cover wide viewport");
        check(scene.sceneRect().contains(node->sceneBoundingRect()) && node->pos()==position,"Canvas expansion moved node or left it outside bounds");
        app.processEvents(); // Drain the watcher's initial queued update before the next resize.
        const auto expanded=scene.sceneRect();
        bounds.updateBounds(); check(scene.sceneRect()==expanded,"Repeated canvas update grew scene unnecessarily");
        host.resize(3600,350); app.processEvents();
        const auto widerResizeCalls=view->resizeCalls;
        bounds.updateBounds();
        check(scene.sceneRect().contains(view->mapToScene(view->viewport()->rect()).boundingRect())
            && view->resizeCalls>widerResizeCalls && view->sceneAtResize==scene.sceneRect(),
            "Further widening did not expand bounds and refresh graph endpoints");
        host.resize(500,350); app.processEvents(); bounds.updateBounds();
        check(scene.sceneRect().contains(expanded),"Narrowing viewport trapped existing nodes again");
        view->scale(.1,.1); bounds.updateBounds();
        check(scene.sceneRect().contains(view->mapToScene(view->viewport()->rect()).boundingRect()),"Zooming out exceeded scene bounds");
        QGraphicsScene next; next.setSceneRect(-1250,-1250,2500,2500); view->setScene(&next);
        bounds.updateBounds();
        check(next.sceneRect().contains(view->mapToScene(view->viewport()->rect()).boundingRect()),"Replacement clip scene not expanded");
        std::puts("PASS: wide and zoomed Nodes viewport covered; node positions retained; no shrinking; scene replacement supported");
        return 0;
    }
    if (app.arguments().contains("--main-placement-test")) {
        QTemporaryDir directory;
        const auto path=directory.filePath("Unbound.json");
        Unbound::PaneSettings settings(path);
        QWidget preserved; preserved.setGeometry(20,30,400,300); preserved.show(); app.processEvents(); const auto preservedGeometry=preserved.saveGeometry();
        settings.save("frameEditPageEffectsLibrary",preservedGeometry);
        QWidget main;
        auto layout=new QVBoxLayout(&main);
        auto pages=new QStackedWidget(&main); pages->setObjectName("widgetStack_Panel");
        layout->addWidget(pages);
        auto edit=new QWidget; edit->setObjectName("WStackPage_Conform"); pages->addWidget(edit);
        auto color=new QWidget; color->setObjectName("WStackPage_Color"); pages->addWidget(color);
        auto dual=new QWidget(&main); dual->setObjectName("pageDualScreenMode"); dual->hide();
        main.setGeometry(50,60,400,260); main.show(); app.processEvents();
        {
            Unbound::MainWindowPlacement placement(settings);
            placement.attach(&main);
            const auto editGeometry=main.geometry();
            pages->setCurrentWidget(color); app.processEvents();
            check(main.geometry()==editGeometry,"New page changed main placement without saved settings");
            main.setGeometry(100,110,500,300); app.processEvents();
            const auto colorGeometry=main.geometry();
            if(QGuiApplication::platformName()==QLatin1String("windows")) {
                main.showMaximized();app.processEvents();
                main.showNormal();main.resize(colorGeometry.width(),colorGeometry.height()+9);app.processEvents();
                check(main.geometry()==colorGeometry,"Normal placement retained a transient maximize/restore size clamp");
                main.showMaximized();app.processEvents();main.showNormal();
                Unbound::WindowInteractionEvent::notify(&main);main.setGeometry(colorGeometry.adjusted(10,10,20,20));app.processEvents();
                check(main.geometry()==colorGeometry.adjusted(10,10,20,20),"Deferred normal placement overrode a newer user gesture");
                main.setGeometry(colorGeometry);app.processEvents();
            }
            pages->setCurrentWidget(edit); app.processEvents();
            check(main.geometry()==editGeometry,"Edit main placement not restored");
            pages->setCurrentWidget(color); app.processEvents();
            check(main.geometry()==colorGeometry,"Color main placement not restored");
            main.showMaximized(); app.processEvents();
            pages->setCurrentWidget(edit); app.processEvents();
            check(!main.isMaximized() && main.geometry()==editGeometry,"Maximized outgoing page damaged normal placement");
            pages->setCurrentWidget(color); app.processEvents();
            check(main.isMaximized(),"Per-page maximized state not restored");
            pages->setCurrentWidget(edit); app.processEvents(); placement.flush();
            const auto savedEdit=settings.geometry("MainWindow/WStackPage_Conform");
            main.showFullScreen(); app.processEvents(); settlePlatformWindowState(main); placement.refresh();
            pages->setCurrentWidget(color); app.processEvents();
            check(main.isFullScreen() && savedEdit==settings.geometry("MainWindow/WStackPage_Conform"),"Fullscreen overwritten or restored by placement tracking");
            main.showNormal(); app.processEvents(); settlePlatformWindowState(main);
            pages->setCurrentWidget(edit); app.processEvents();
            dual->show(); placement.refresh();
            main.setGeometry(130,140,440,270); app.processEvents();
            const auto dualGeometry=main.geometry();
            pages->setCurrentWidget(color); app.processEvents();
            check(main.geometry()==dualGeometry && !main.isMaximized(),"Dual-screen placement changed on page switch");
            check(savedEdit==settings.geometry("MainWindow/WStackPage_Conform"),"Dual-screen mode overwrote normal placement");
            dual->hide(); pages->setCurrentWidget(edit); app.processEvents();
            placement.refresh(); placement.flush();
        }
        const auto expectedNative=nativeWindowBounds(main);
        main.setGeometry(200,210,350,240); app.processEvents();
        prepareNativePlacementTest(main, settings);
        Unbound::PaneSettings reloaded(path);
        Unbound::MainWindowPlacement restored(reloaded); restored.attach(&main);
        app.processEvents();
        check(main.size()==QSize(400,260),"Placement did not survive settings reload");
        verifyNativePlacementTest(main, expectedNative);
        check(reloaded.geometry("frameEditPageEffectsLibrary")==preservedGeometry,"Main placement erased pane settings");
        check(Unbound::MainWindowPlacement::reachable(QRect(2500,50,500,300),{QRect(0,0,800,600)})==QRect(300,50,500,300),"Disconnected monitor placement not clamped");
        check(Unbound::MainWindowPlacement::reachable(QRect(-700,50,500,300),{QRect(-800,0,800,600),QRect(0,0,800,600)})==QRect(-700,50,500,300),"Valid negative monitor placement changed");
        std::puts("PASS: per-page main placement, new-page preservation, maximized state, persistence, fullscreen/dual-screen guards and monitor recovery");
        return 0;
    }
    {
        {
            const QList<QRect> areas{QRect(-800,0,800,600),QRect(0,0,800,600)};
            check(Unbound::reachablePaneGeometry(QRect(-100,40,400,300),areas)==QRect(-100,40,400,300),
                "Reachable pane spanning adjacent monitors was changed");
            check(Unbound::reachablePaneGeometry(QRect(-810,-10,400,300),areas)==QRect(-800,0,400,300),
                "Partially off-screen pane did not recover its top-left controls");
            check(Unbound::reachablePaneGeometry(QRect(740,550,400,300),areas)==QRect(400,300,400,300),
                "Pane at bottom-right was not brought inside the work area");
            check(Unbound::reachablePaneGeometry(QRect(-50,20,400,300),{QRect(-800,0,700,600),QRect(0,0,800,600)})
                ==QRect(0,20,400,300),"Monitor gap was treated as usable pane space");
            QWidget constrained;
            const auto area=QGuiApplication::primaryScreen()->availableGeometry();
            constrained.setMinimumWidth(area.width()+100);
            constrained.setGeometry(area.left()-10,area.top()-10,400,200);
            Unbound::ensureReachablePaneGeometry(&constrained);
            check(constrained.x()==area.left() && constrained.y()==area.top()
                && constrained.width()>=constrained.minimumWidth(),
                "Oversized native minimum hid top-left controls or was overridden");
            std::puts("PASS: pane reachability, adjacent monitors, gaps and oversized native minimums");
        }
        {
            QTemporaryDir refsDir;Unbound::PaneSettings refs(refsDir.filePath("refs.json"));
            const QRect paneRect(48,72,240,480),mainRect(400,0,1000,900);
            const auto paneJson=QJsonObject{{"clientRect",Unbound::rectangleJson(paneRect)}};
            const auto mainJson=QJsonObject{{"clientRect",Unbound::rectangleJson(mainRect)}};
            for(const auto &key:{QString("frameEditPageInspector"),QString("Audio/frameInspectorContainer"),QString("Color/m_pWidgetGallery")}) {
                check(refs.save(key,Unbound::qtGeometryBytes(paneJson)).isEmpty(),"Reference geometry write failed");
                check(refs.saveFloating(key,true).isEmpty(),"Reference floating state write failed");
            }
            check(refs.save("MainWindow/WStackPage_Audio",Unbound::qtGeometryBytes(mainJson)).isEmpty(),"Reference main write failed");
            check(refs.save("MainWindow/WStackPage_Color",Unbound::qtGeometryBytes(mainJson)).isEmpty(),"Duplicate main write failed");
            check(refs.save("frameEditPageMediaPool",Unbound::qtGeometryBytes(QJsonObject{{"clientRect",Unbound::rectangleJson(QRect(1,2,300,400))}})).isEmpty(),"Current-page fixture write failed");
            refs.saveFloating("frameEditPageMediaPool",true);
            // Reload models pages not visited in this session. Saved layouts are
            // deliberately not enumerated: only the last working arrangement.
            Unbound::PaneSettings loaded(refsDir.filePath("refs.json"));
            const QList<QRect> areas{QRect(0,0,1920,1080)};
            auto resolveMain=[&](const QJsonObject &value){return Unbound::gridSavedRect(value,areas);};
            auto rectangles=Unbound::gridPageReferences(loaded,"WStackPage_Conform",areas,resolveMain);
            check(rectangles.size()==2 && rectangles.contains(paneRect) && rectangles.contains(mainRect),
                "Grid references omitted unvisited pages/main, included current page or duplicated outlines");
            loaded.saveFloating("Audio/frameInspectorContainer",false);loaded.saveFloating("Color/m_pWidgetGallery",false);
            rectangles=Unbound::gridPageReferences(loaded,"WStackPage_Conform",areas,resolveMain);
            check(rectangles.size()==1 && rectangles.first()==mainRect,"Docked panes appeared as references");
            check(Unbound::gridSavedRect(QJsonObject{{"clientRect",Unbound::rectangleJson(QRect(-2000,0,240,400))}},areas)==QRect(0,0,240,400),
                "Reference geometry did not recover a removed monitor");
            check(Unbound::gridSavedRect(QJsonObject{{"clientRect",Unbound::rectangleJson(paneRect)},{"maximized",true}},areas)==areas.first(),
                "Maximized reference did not use work area");
            std::puts("PASS: other-page references, main placement, unvisited settings, deduplication, docking exclusion and monitor recovery");
        }
        {
            const QRect rect(20,20,60,60),area(0,0,100,100);
            const auto single=Unbound::gridReferenceImage({rect},area,1);
            const auto duplicate=Unbound::gridReferenceImage({rect,rect},area,1);
            const auto neighbor=Unbound::gridReferenceImage({rect.translated(2,0)},area,1);
            const auto combined=Unbound::gridReferenceImage({rect,rect.translated(2,0)},area,1);
            check(single==duplicate,"Duplicate outlines increased intensity");
            for(int y=0;y<100;++y)for(int x=0;x<100;++x)
                check(qAlpha(combined.pixel(x,y))==qMax(qAlpha(single.pixel(x,y)),qAlpha(neighbor.pixel(x,y))),
                    "Nearby outlines increased intensity above the stronger individual contribution");
            check(qAlpha(single.pixel(50,19))==qAlpha(single.pixel(50,20)) &&
                qAlpha(single.pixel(50,20))>qAlpha(single.pixel(50,22)) &&
                qAlpha(single.pixel(50,22))>qAlpha(single.pixel(50,24)),"Outline falloff is not smooth and centered");
            check(qAlpha(single.pixel(20,20))<qAlpha(single.pixel(30,20)),"Reference corners were not rounded");
            const auto retina=Unbound::gridReferenceImage({rect},area,2);
            check(retina.size()==QSize(200,200) && retina.devicePixelRatio()==2,"Outline raster did not scale for high DPI");
            std::puts("PASS: rounded outline raster, smooth symmetric falloff, max-opacity overlap and DPI scaling");
        }
        Unbound::PaneGridSnap grid;
        const QRect work(-1920,40,1920,1040);
        const QSize min(1,1),max(16777215,16777215);
        QPoint engaged;
        auto gridMove=[&](int x) {return grid.offset(QRect(x,49,101,79),work,24,true,true,true,true,min,max,&engaged);};
        check(gridMove(-1915)==QPoint(-5,-9) && engaged==QPoint(1,1),"Grid work-area origin or negative monitor coordinates failed");
        check(gridMove(-1907).x()==-13,"Grid midpoint hysteresis lost previous line");
        check(gridMove(-1905).x()==9,"Grid failed to advance to next line");
        check(grid.takeChanged()==QPoint(1,1) && grid.takeChanged().isNull(),"Grid retarget notification failed");
        grid.reset();
        check(grid.offset(QRect(0,0,250,100),QRect(0,0,1920,1080),24,false,true,false,false,
            QSize(245,1),QSize(300,1000)).x()==14,"Grid did not skip a line below minimum width");
        grid.reset();
        check(grid.offset(QRect(0,0,250,100),QRect(0,0,1920,1080),24,false,true,false,false,
            QSize(1,1),QSize(250,1000)).x()==-10,"Grid exceeded maximum width");
        grid.reset();
        check(grid.offset(QRect(0,0,250,100),QRect(0,0,1920,1080),24,false,true,false,false,
            QSize(250,1),QSize(250,1000),&engaged).isNull() && !engaged.x(),"Grid forced an impossible fixed-width snap");
        grid.reset();
        check(grid.offset(QRect(11,11,229,229),QRect(0,0,1920,1080),24,true,false,true,false,
            QSize(240,240),max)==QPoint(-11,-11),"Grid leading resize changed fixed opposite edges");
        grid.reset();
        check(grid.offset(QRect(53,73,500,400),QRect(0,0,3840,2160),48,true,true,true,true,min,max)==QPoint(-5,23),
            "DPI-scaled grid spacing failed");
        check(Unbound::paneGridArea(QPoint(-100,100),{work,QRect(0,0,1920,1080)})==work,"Grid selected wrong monitor");
        std::puts("PASS: grid spacing, negative work-area origins, midpoint hysteresis, resize limits, fixed sizes and DPI units");
        using Unbound::paneSnapOffset;
        Unbound::PaneSnapTransition transition;
        check(transition.update(QPoint(-12,0),QPoint(1,0),0).isNull() && transition.animating(),
            "Snap entry jumped immediately instead of beginning a transition");
        check(transition.update(QPoint(-12,0),QPoint(1,0),8)==QPoint(-1,0),
            "Snap entry did not begin with a gentle correction");
        check(transition.update(QPoint(-12,0),QPoint(1,0),16)==QPoint(-3,0),
            "Snap correction did not accelerate towards exact contact");
        check(transition.update(QPoint(-12,0),QPoint(1,0),32)==QPoint(-12,0) && !transition.animating(),
            "Snap entry failed to reach exact alignment and stop animation");
        check(transition.update(QPoint(-5,0),QPoint(1,0),100)==QPoint(-5,0) && !transition.animating(),
            "An established snap lagged behind changing pointer proposals");
        check(transition.update(QPoint(),QPoint(),110)==QPoint(-5,0) && transition.animating(),
            "Snap release jumped back to the raw pointer proposal");
        const auto releasing=transition.update(QPoint(),QPoint(),126);
        check(releasing.x()>-5 && releasing.x()<0,"Snap release skipped its intermediate correction");
        check(transition.update(QPoint(8,0),QPoint(1,0),126)==releasing,
            "Re-entering a snap zone during release introduced a correction jump");
        check(transition.update(QPoint(8,0),QPoint(1,0),158)==QPoint(8,0) && !transition.animating(),
            "Interrupted release failed to settle at the new snap correction");
        transition.update(QPoint(),QPoint(),250);
        check(transition.update(QPoint(),QPoint(),282).isNull() && !transition.animating(),
            "Released window retained an offset or running animation");
        transition.reset();
        transition.update(QPoint(12,0),QPoint(1,0),0);
        check(transition.update(QPoint(12,-12),QPoint(1,1),8)==QPoint(1,0),
            "Beginning one axis restarted the other axis's transition");
        check(transition.update(QPoint(12,-12),QPoint(1,1),16)==QPoint(3,-1),
            "Independent snap axes did not advance independently");
        QPoint engagement;
        check(paneSnapOffset(QRect(100,500,80,80),{QRect(100,100,200,200)},12,
            true,true,true,true,QSize(),QSize(16777215,16777215),{},&engagement).isNull()
            && engagement==QPoint(1,0),"Exact edge alignment was confused with leaving the snap zone");
        std::puts("PASS: snap entry/release easing, interrupted release, exact lock, independent axes and stopped animation");
        Unbound::PaneDragProposal proposal;
        QRect snapped(300, 140, 80, 80);
        proposal.update(snapped, QPoint(500, 200), true, true, true, true);
        for (int step = 1; step <= 20; ++step) {
            const auto raw = proposal.update(snapped, QPoint(500 + step, 200), true, true, true, true);
            snapped = raw.translated(paneSnapOffset(raw, {QRect(100, 100, 200, 200)}, 12));
        }
        check(snapped.x() == 320, "Native drag feedback trapped window at snapped position");
        proposal.reset();
        snapped = QRect(20, 140, 80, 80);
        proposal.update(snapped, QPoint(100, 200), false, true, false, false);
        for (int step = 1; step <= 20; ++step) {
            auto raw = proposal.update(snapped, QPoint(100 + step, 200), false, true, false, false);
            const auto offset = paneSnapOffset(raw, {QRect(100, 100, 200, 200)}, 12, false, true, false, false);
            raw.setWidth(raw.width() + offset.x());
            snapped = raw;
        }
        check(snapped.x() == 20 && snapped.width() == 100, "Native resize feedback trapped moving edge");
        const QVector<QRect> targets{QRect(100, 100, 200, 200)};
        const QVector<QRect> displays{QRect(0, 0, 1920, 1040), QRect(-1280, -200, 1280, 1024)};
        check(paneSnapOffset(QRect(7, 9, 400, 300), displays, 12) == QPoint(-7, -9), "Display top-left snap failed");
        check(paneSnapOffset(QRect(1513, 733, 400, 300), displays, 12) == QPoint(7, 7), "Display work-area bottom-right snap failed");
        check(paneSnapOffset(QRect(-1273, -193, 400, 300), displays, 12) == QPoint(-7, -7), "Secondary display edge snap failed");
        check(paneSnapOffset(QRect(100, 100, 1813, 933), displays, 12, false, true, false, true) == QPoint(7, 7), "Resize to display corner failed");
        check(paneSnapOffset(QRect(100, 100, 1813, 933), displays, 12, false, true, false, true, QSize(1,1), QSize(1815,935)).isNull(), "Display resize ignored size limits");
        check(paneSnapOffset(QRect(13, 20, 400, 300), displays, 12).isNull(), "Display edge failed to release");
        std::puts("PASS: display work-area move/resize snapping, secondary display, constraints and release distance");
        check(paneSnapOffset(QRect(307, 140, 80, 80), targets, 12) == QPoint(-7, 0), "Side adjacency snap failed");
        check(paneSnapOffset(QRect(15, 140, 80, 80), targets, 12) == QPoint(5, 0), "Opposite side snap failed");
        check(paneSnapOffset(QRect(140, 309, 80, 80), targets, 12) == QPoint(0, -9), "Vertical snap failed");
        check(paneSnapOffset(QRect(105, 103, 80, 80), targets, 12) == QPoint(-5, -3), "Edge alignment snap failed");
        check(paneSnapOffset(QRect(315, 140, 80, 80), targets, 12).isNull(), "Window failed to pull free of snap radius");
        check(paneSnapOffset(QRect(307, 500, 80, 80), targets, 12)==QPoint(-7,0), "Distant window edge did not align");
        const QRect moving(500,500,80,80);
        const QRect nearWindow(100,300,200,275),farWindow(1500,300,200,285);
        check(paneSnapOffset(moving,{farWindow,nearWindow},12)==QPoint(0,-5) &&
            paneSnapOffset(moving,{nearWindow,farWindow},12)==QPoint(0,-5),
            "Equally close edge candidates did not favour the nearer window independently of enumeration order");
        check(paneSnapOffset(moving,{nearWindow,QRect(1500,300,200,276)},12)==QPoint(0,-4),
            "Spatial proximity overrode the smaller edge adjustment");
        check(paneSnapOffset(moving,{nearWindow,nearWindow,QRect(1500,300,200,275)},12)==QPoint(0,-5),
            "Duplicate alignment edges changed the snap result");
        check(paneSnapOffset(moving,{QRect(1500,300,200,287)},12)==QPoint(0,7),
            "Bottom edges did not align across a horizontal gap");
        check(paneSnapOffset(moving,{QRect(1500,507,200,200)},12,false,false,true,false)==QPoint(0,7),
            "Top-edge resize did not align across a horizontal gap");
        check(paneSnapOffset(moving,{QRect(1500,300,200,287)},12,false,false,false,true)==QPoint(0,7),
            "Bottom-edge resize did not align across a horizontal gap");
        check(paneSnapOffset(moving,{QRect(507,2000,200,200)},12,true,false,false,false)==QPoint(7,0),
            "Left-edge resize did not align across a vertical gap");
        check(paneSnapOffset(moving,{QRect(587,2000,200,200)},12,false,true,false,false)==QPoint(7,0),
            "Right-edge resize did not align across a vertical gap");
        check(paneSnapOffset(moving,{QRect(587,2000,200,200),QRect(1500,300,200,287)},12,
            false,true,false,true)==QPoint(7,7),"Distant corner resizing did not align independently on both axes");
        check(paneSnapOffset(moving,{QRect(587,2000,200,200)},12,false,true,false,false,QSize(1,1),QSize(85,100)).isNull(),
            "Distant edge alignment violated resize limits");
        check(paneSnapOffset(moving,{QRect(593,2000,200,200)},12).isNull(),
            "Distant alignment did not release outside the snap radius");
        check(paneSnapOffset(QRect(-500,-500,80,80),{QRect(-1500,-700,200,287)},12)==QPoint(0,7),
            "Distant alignment failed at negative display coordinates");
        check(paneSnapOffset(moving,{},12,true,true,true,true,QSize(),QSize(16777215,16777215),
            {QRect(507,2000,1000,800)}).isNull(),"Distant monitor edge became an unrelated alignment guide");
        std::puts("PASS: distant edge/corner alignment, spatial tie-breaking, duplicate edges, limits, release and local work areas");
        check(paneSnapOffset(QRect(-293, -160, 80, 80), {QRect(-500, -200, 200, 200)}, 12) == QPoint(-7, 0),
            "Negative monitor coordinates failed");
        check(paneSnapOffset(QRect(307, 140, 80, 80), {QRect(100, 100, 200, 200), QRect(309, 100, 200, 200)}, 12)
            == QPoint(2, 0), "Nearest candidate not selected");
        std::puts("PASS: magnetic edge adjacency/alignment, nearest target, release distance and negative screen coordinates");
        check(paneSnapOffset(QRect(20, 140, 73, 80), targets, 12, false, true, false, false) == QPoint(7, 0),
            "Right-edge resize did not snap");
        check(paneSnapOffset(QRect(307, 140, 80, 80), targets, 12, true, false, false, false) == QPoint(-7, 0),
            "Left-edge resize did not snap");
        check(paneSnapOffset(QRect(140, 307, 80, 80), targets, 12, false, false, true, false) == QPoint(0, -7),
            "Top-edge resize did not snap");
        check(paneSnapOffset(QRect(20, 20, 73, 73), targets, 12, false, true, false, true) == QPoint(7, 7),
            "Corner resize did not snap both axes");
        check(paneSnapOffset(QRect(20, 140, 73, 80), targets, 12, false, true, false, false, QSize(1, 1), QSize(75, 90)).isNull(),
            "Resize snap violated maximum size");
        check(paneSnapOffset(QRect(20, 140, 87, 80), targets, 12, false, true, false, false, QSize(85, 1)).isNull(),
            "Resize snap violated minimum size");
        check(paneSnapOffset(QRect(105, 140, 80, 80), targets, 12, false, true, false, false).isNull(),
            "Resize incorrectly snapped the stationary edge");
    }
    if (runPlatformWindowTests(app)) return 0;
    if (app.arguments().contains("--shutdown-test") || app.arguments().contains("--close-all-test")) {
        QWidget main;
        auto pane = new QWidget(&main);
        auto paneLayout = new QVBoxLayout(pane);
        paneLayout->addWidget(new QWidget(pane));
        main.show(); pane->show(); app.processEvents();
        Unbound::SidebarFloatSession session;
        check(session.start(pane, "Shutdown", &main).isEmpty(), "Shutdown setup failed");
        check(!session.floatingWindow()->testAttribute(Qt::WA_QuitOnClose), "Helper participates in last-window exit accounting");
        bool quit = false;
        QObject::connect(&app, &QCoreApplication::aboutToQuit, [&]() { quit = true; });
        QTimer::singleShot(0, [&]() {
            if (app.arguments().contains("--close-all-test")) QApplication::closeAllWindows();
            else main.close();
        });
        QTimer::singleShot(1500, [&]() { app.exit(77); });
        const int result = app.exec();
        check(result == 0 && quit, "Main window close did not exit naturally");
        std::puts("PASS: application exits naturally with a floating pane (no forced quit)");
        return 0;
    }
    if (app.arguments().contains("--standalone-test")) {
        QTemporaryDir settingsDirectory(QDir::currentPath() + "/unbound-settings-test-XXXXXX");
        check(settingsDirectory.isValid(), "Cannot create settings test directory");
        const auto settingsPath = settingsDirectory.filePath("Unbound.json");
        qputenv("UNBOUND_SETTINGS_FILE", settingsPath.toUtf8());
        QWidget sample;
        sample.setGeometry(120, 90, 440, 320);
        // Migration from the geometry-only release must preserve placement.
        QFile legacy(settingsPath);
        check(legacy.open(QIODevice::WriteOnly), "Cannot create legacy settings");
        legacy.write(QJsonDocument(QJsonObject{{"version", 1}, {"panes", QJsonObject{
            {"frameEditPageMediaPool", QString::fromLatin1(sample.saveGeometry().toBase64())}}}}).toJson());
        legacy.close();
        Unbound::PaneSettings seed(settingsPath);
        check(seed.geometry("frameEditPageMediaPool") == sample.saveGeometry()
            && !seed.floating("frameEditPageMediaPool"), "Legacy settings migration failed");
        check(seed.save("frameEditPageMediaPool", sample.saveGeometry()).isEmpty(), "Cannot seed saved placement");
        check(seed.saveFloating("frameEditPageMediaPool", true).isEmpty(), "Cannot seed floating intent");
        QWidget main;
        main.setObjectName("UiMainWindow");
        auto page = new QWidget(&main);
        page->setObjectName("frameEditPageWidgetsContainer");
        auto pane = new QWidget(page);
        pane->setObjectName("frameEditPageMediaPool");
        auto original = new QVBoxLayout(pane);
        original->addWidget(new QPushButton("Live control", pane));
        const bool selectedColor=app.arguments().contains("--color-selected-test");
        const bool restoreToggles=app.arguments().contains("--toggle-state-test") || selectedColor;
        auto poolToggle=new QPushButton("Media Pool",page);
        poolToggle->setObjectName("btnEditToggleMediaPoolInSSM");
        poolToggle->setCheckable(true); poolToggle->setChecked(true);
        QObject::connect(poolToggle,&QPushButton::clicked,page,[=](bool checked) {pane->setVisible(checked);});
        if (restoreToggles) {
            check(seed.saveToggle("frameEditPageMediaPool",false).isEmpty(),"Cannot seed pool toggle");
            check(seed.saveToggle("frameEditAudioMeter",false).isEmpty(),"Cannot seed Mixer toggle");
            check(seed.saveToggle("frameEditPageInspector",false).isEmpty(),"Cannot seed Inspector toggle");
        }
        auto mixerSplit = new QSplitter(page);
        new QWidget(mixerSplit);
        auto savedMixer = new QWidget(mixerSplit);
        savedMixer->setObjectName("frameEditAudioMeter");
        auto savedMixerLayout = new QVBoxLayout(savedMixer);
        auto lazyMixerContent = new QLabel("Initialized Mixer", savedMixer);
        savedMixerLayout->addWidget(lazyMixerContent);
        auto alternateMeter = new QLabel("Alternate meter", savedMixer);
        savedMixerLayout->addWidget(alternateMeter);
        lazyMixerContent->hide(); alternateMeter->hide(); savedMixer->hide();
        auto mixerToggle = new QPushButton("Mixer", page);
        mixerToggle->setObjectName("btnEditToggleMixerInSSM");
        mixerToggle->setCheckable(true);
        int mixerActivations = 0;
        QObject::connect(mixerToggle, &QPushButton::clicked, page, [=, &mixerActivations](bool checked) {
            ++mixerActivations;
            QTimer::singleShot(0, page, [=] {
                if (checked || lazyMixerContent->parentWidget()==savedMixer) lazyMixerContent->setVisible(checked);
                savedMixer->setVisible(checked);
            });
        });
        check(seed.saveFloating("frameEditAudioMeter", true).isEmpty(), "Cannot seed Mixer intent");
        auto savedInspector = new QWidget(page);
        savedInspector->setObjectName("frameEditPageInspector");
        auto inspectorLayout = new QVBoxLayout(savedInspector);
        auto inspectorContent = new QLabel("Inspector selection", savedInspector);
        inspectorLayout->addWidget(inspectorContent);
        auto inspectorToggle = new QPushButton("Inspector", page);
        inspectorToggle->setObjectName("btnEditToggleInspectorInSSM");
        inspectorToggle->setCheckable(true);
        const bool inspectorInitiallyActive = app.arguments().contains("--visible-start");
        inspectorToggle->setChecked(inspectorInitiallyActive);
        inspectorContent->setVisible(inspectorInitiallyActive);
        savedInspector->setVisible(inspectorInitiallyActive);
        int inspectorActivations = 0;
        QObject::connect(inspectorToggle, &QPushButton::clicked, page, [=, &inspectorActivations](bool checked) {
            ++inspectorActivations;
            QTimer::singleShot(0, page, [=] {
                if (checked || inspectorContent->parentWidget()==savedInspector) inspectorContent->setVisible(checked);
                savedInspector->setVisible(checked);
            });
        });
        check(seed.saveFloating("frameEditPageInspector", true).isEmpty(), "Cannot seed Inspector intent");
        auto savedMetadata = new QWidget(page);
        savedMetadata->setObjectName("frameEditPageMetadata");
        auto metadataLayout = new QHBoxLayout(savedMetadata);
        auto metadataContent = new QPushButton("Editable metadata", savedMetadata);
        metadataLayout->addWidget(metadataContent);
        metadataContent->hide(); savedMetadata->hide();
        auto metadataToggle = new QPushButton("Metadata", page);
        metadataToggle->setObjectName("btnEditToggleMetadataInSSM");
        metadataToggle->setCheckable(true);
        int metadataActivations = 0;
        int metadataEdits = 0;
        QObject::connect(metadataContent, &QPushButton::clicked, page, [&metadataEdits] { ++metadataEdits; });
        QObject::connect(metadataToggle, &QPushButton::clicked, page, [=, &metadataActivations](bool checked) {
            ++metadataActivations;
            QTimer::singleShot(0, page, [=] {
                metadataContent->setVisible(checked);
                savedMetadata->setVisible(checked);
            });
        });
        check(seed.saveFloating("frameEditPageMetadata", !inspectorInitiallyActive).isEmpty(), "Cannot seed Metadata intent");
        auto colorPage = new QWidget(&main);
        colorPage->setObjectName("WStackPage_Color");
        auto nodes = new QWidget(colorPage);
        nodes->setObjectName("m_pFrameNodeGraph");
        auto nodesGrid = new QGridLayout(nodes);
        auto nodeControl = new QPushButton("Graph control", nodes);
        nodesGrid->addWidget(nodeControl, 1, 2, 2, 3);
        nodesGrid->setColumnStretch(2, 3);
        nodesGrid->setRowStretch(1, 2);
        auto nodesToggle = new QPushButton("Nodes", colorPage);
        nodesToggle->setObjectName("btnColorToggleNodeGraph");
        nodesToggle->setCheckable(true);
        nodeControl->hide(); nodes->hide(); colorPage->hide();
        int nodeEdits = 0;
        QObject::connect(nodeControl, &QPushButton::clicked, colorPage, [&nodeEdits] { ++nodeEdits; });
        QObject::connect(nodesToggle, &QPushButton::clicked, colorPage, [=](bool checked) {
            QTimer::singleShot(0, colorPage, [=] {
                // Model Resolve retaining and repopulating its original grid.
                if (checked) nodesGrid->addWidget(nodeControl, 1, 2, 2, 3);
                else nodeControl->setParent(colorPage);
                nodes->setVisible(checked); nodeControl->setVisible(checked);
            });
        });
        check(seed.saveFloating("Color/m_pFrameNodeGraph", true).isEmpty(), "Cannot seed Color Nodes intent");
        if (restoreToggles) seed.saveToggle("Color/m_pFrameNodeGraph",false);
        const QStringList colorNames={"m_pWidgetGallery","m_pWidgetLutBrowser","m_pWidgetMediaPool"};
        const QStringList colorToggleNames={"btnColorToggleGallery","btnColorToggleLutsBrowser","btnColorToggleMediaPool"};
        auto colorSidebar=new QStackedWidget(colorPage);
        QVector<QWidget *> colorSlots;
        QVector<QPushButton *> colorControls, colorToggles;
        QVector<QLayout *> colorLayouts;
        QVector<int> colorActivations(3,0), colorEdits(3,0);
        for (int i=0;i<3;++i) {
            auto slot=new QWidget(colorPage); slot->setObjectName(colorNames[i]);
            QLayout *layout=new QGridLayout(slot);
            auto control=new QPushButton("Native pane content",slot); layout->addWidget(control); control->hide();
            colorSidebar->addWidget(slot);
            auto toggle=new QPushButton("Native selector",colorPage); toggle->setObjectName(colorToggleNames[i]); toggle->setCheckable(true);
            colorSlots.append(slot); colorControls.append(control); colorLayouts.append(layout); colorToggles.append(toggle);
            QObject::connect(control,&QPushButton::clicked,colorPage,[&,i] { ++colorEdits[i]; });
            check(seed.saveFloating("Color/"+colorNames[i],true).isEmpty(),"Cannot seed Color pane intent");
            if (restoreToggles) seed.saveToggle("Color/"+colorNames[i],selectedColor && i==0);
        }
        for (int i=0;i<3;++i) QObject::connect(colorToggles[i],&QPushButton::clicked,colorPage,[&,i](bool checked) {
            ++colorActivations[i];
            QTimer::singleShot(0,colorPage,[&,i,checked] {
                if (checked) {
                    for (int j=0;j<3;++j) if (j!=i) colorToggles[j]->setChecked(false);
                    colorSidebar->setCurrentWidget(colorSlots[i]); colorSidebar->show();
                    colorLayouts[i]->addWidget(colorControls[i]);
                } else colorSidebar->hide();
                colorControls[i]->setVisible(checked);
            });
        });
        // Unsupported Clips must ignore legacy floating intent, even on Color.
        auto unrelatedLightBox=new QWidget(colorPage); unrelatedLightBox->setObjectName("m_pLightBox");
        auto clipsLayout=new QHBoxLayout(unrelatedLightBox);
        auto clipsContent=new QLabel("Native Clips",unrelatedLightBox); clipsLayout->addWidget(clipsContent);
        check(seed.saveFloating("Color/m_pLightBox",true).isEmpty(),"Cannot seed legacy Clips intent");

        main.show(); page->show(); pane->show(); app.processEvents();
        const bool visibleStart = app.arguments().contains("--visible-start");
        if (!visibleStart) page->hide();
        QLibrary helper(app.arguments().last());
        helper.setLoadHints(QLibrary::PreventUnloadHint);
        check(helper.load(), qPrintable(helper.errorString()));
        auto start = resolveBootstrap(helper);
        check(start && start(nullptr) == 1 && start(nullptr) == 1, "Standalone bootstrap failed or duplicate start rejected");
        app.processEvents();
        auto buttons = pane->findChildren<QToolButton *>("resolvePaneUndockButton");
        check(buttons.size() == 1, "Standalone discovery did not create exactly one button");
        if (!visibleStart) {
            check(pane->layout() == original, "Startup restored a pane on an inactive page");
            page->show();
            QEventLoop ready;
            QTimer::singleShot(1200, &ready, &QEventLoop::quit);
            ready.exec();
        }
        QWidget *floating = nullptr;
        for (auto widget : QApplication::topLevelWidgets())
            if (widget->objectName() == "unboundFloating_frameEditPageMediaPool") floating = widget;
        check(floating && floating->layout() == original, "Standalone Undock failed");
        check(floating->size() == sample.size(), "Standalone startup did not recall saved dimensions");
        floating->resize(480, 350);
        app.processEvents();
        QEventLoop settle;
        QTimer::singleShot(950, &settle, &QEventLoop::quit);
        settle.exec();
        Unbound::PaneSettings autosaved(settingsPath);
        QWidget autoRecalled;
        check(autoRecalled.restoreGeometry(autosaved.geometry("frameEditPageMediaPool")) && autoRecalled.size() == QSize(480, 350), "Placement not autosaved while floating");
        if (visibleStart) check(floating->close(), "Standalone close rejected");
        else {
            check(buttons[0]->isChecked() && buttons[0]->toolTip()=="Click to dock; drag to move","Restored pane toggle not synchronized");
            buttons[0]->click();
        }
        app.processEvents();
        check(pane->layout() == original, "Standalone restore failed");
        check(!buttons[0]->isChecked() && buttons[0]->toolTip()=="Undock","Docked toggle not synchronized after X or button");
        Unbound::PaneSettings reloaded(settingsPath);
        check(!reloaded.floating("frameEditPageMediaPool"), "User docking was not persisted");
        QWidget recalled;
        check(recalled.restoreGeometry(reloaded.geometry("frameEditPageMediaPool")) && recalled.size() == QSize(480, 350), "Changed placement not persisted to disk");
        check(reloaded.geometry("frameEditPageSoundLibrary").isEmpty(), "Placement leaked into another pane");
        QEventLoop dockWait;
        QTimer::singleShot(1100, &dockWait, &QEventLoop::quit);
        dockWait.exec();
        check(pane->layout() == original, "Discovery undid explicit docking");
        if (visibleStart) {
            check(savedMetadata->layout() == metadataLayout && metadataActivations == 0,
                "Docked Metadata was activated automatically");
            metadataToggle->click(); app.processEvents();
            auto metadataButton = savedMetadata->findChild<QToolButton *>("resolvePaneUndockButton");
            check(metadataButton && metadataButton->isVisible(), "Metadata Undock button missing");
            metadataButton->click(); app.processEvents();
        }
        QWidget *restoredMetadata = nullptr;
        for (auto widget : QApplication::topLevelWidgets())
            if (widget->objectName() == "unboundFloating_frameEditPageMetadata") restoredMetadata = widget;
        check(restoredMetadata && restoredMetadata->layout() == metadataLayout && metadataContent->isVisible()
            && metadataActivations == (visibleStart ? 2 : 1), "Metadata detach/activation failed");
        metadataContent->click();
        check(metadataEdits == 1, "Floating Metadata lost live connections");
        check(Unbound::PaneSettings(settingsPath).floating("frameEditPageMetadata"), "Metadata floating intent not saved");
        check(restoredMetadata->close() && savedMetadata->layout() == metadataLayout,
            "Metadata did not restore original layout");
        check(!Unbound::PaneSettings(settingsPath).floating("frameEditPageMetadata"), "Metadata docked intent not saved");
        std::puts("PASS: Metadata discovery, manual/automatic detach, native initialization, live controls and persisted docking");
        QWidget *restoredMixer = nullptr;
        for (auto widget : QApplication::topLevelWidgets())
            if (widget->objectName() == "unboundFloating_frameEditAudioMeter") restoredMixer = widget;
        check(restoredMixer && restoredMixer->layout() == savedMixerLayout && lazyMixerContent->isVisible(),
            "Automatic Mixer restoration did not initialize native content");
        check(mixerActivations == (restoreToggles ? 2 : 1) && alternateMeter->isHidden() && savedMixer->isHidden(),
            "Mixer activation repeated, exposed alternate content, or retained timeline slot");
        check(restoredMixer->close(), "Initialized Mixer did not dock");
        check(savedMixer->layout() == savedMixerLayout && !savedMixer->isHidden(), "Mixer failed to reclaim its docked slot");
        QWidget *restoredInspector = nullptr;
        for (auto widget : QApplication::topLevelWidgets())
            if (widget->objectName() == "unboundFloating_frameEditPageInspector") restoredInspector = widget;
        check(restoredInspector && restoredInspector->layout() == inspectorLayout && inspectorContent->isVisible(),
            "Automatic Inspector restoration did not initialize native content");
        check(inspectorActivations == (inspectorInitiallyActive ? 0 : 1) + (restoreToggles ? 1 : 0)
            && inspectorToggle->isChecked() == !restoreToggles,
            "Inspector activation repeated or toggled an already active pane off");
        if (restoreToggles) {
            check(savedInspector->isHidden() && mixerToggle->isChecked() && poolToggle->isChecked(),"Docking did not show previously reclaimed panes");
            Unbound::PaneSettings persisted(settingsPath);
            check(persisted.hasToggle("frameEditPageInspector") && !persisted.toggle("frameEditPageInspector")
                && persisted.toggle("frameEditAudioMeter") && persisted.toggle("frameEditPageMediaPool"),"Docking did not persist toolbar visibility");
            inspectorToggle->click(); app.processEvents();
            check(Unbound::PaneSettings(settingsPath).toggle("frameEditPageInspector"),"User toolbar on choice not saved");
            inspectorToggle->click(); app.processEvents();
            check(!Unbound::PaneSettings(settingsPath).toggle("frameEditPageInspector") && inspectorContent->isVisible(),"User toolbar off choice lost or floating content hidden");
            std::puts("PASS: persisted Edit toolbar states restore after native initialization; user changes persist");
        }
        check(restoredInspector->close() && savedInspector->layout() == inspectorLayout,
            "Initialized Inspector did not dock");
        check(!Unbound::PaneSettings(settingsPath).floating("frameEditPageInspector"), "Inspector docking was not persisted");
        std::puts("PASS: Mixer and Inspector initialize before restoration; already-active Inspector is not toggled");
        buttons[0]->click(); app.processEvents();
        check(pane->layout() != original, "Repeat undock failed");
        check(nodes->layout() == nodesGrid && !nodesToggle->isChecked(), "Color Nodes restored while Edit was active");
        page->hide(); colorPage->show();
        QEventLoop colorReady;
        QTimer::singleShot(1200, &colorReady, &QEventLoop::quit);
        colorReady.exec();
        QWidget *floatingNodes = nullptr;
        QWidget *floatingEdit = nullptr;
        for (auto widget : QApplication::topLevelWidgets()) {
            if (widget->objectName() == "unboundFloating_m_pFrameNodeGraph") floatingNodes = widget;
            if (widget->objectName() == "unboundFloating_frameEditPageMediaPool") floatingEdit = widget;
        }
        check(floatingNodes && floatingNodes->isVisible() && nodes->layout() == nodesGrid
            && nodeControl->parentWidget() == floatingNodes && nodeControl->isVisible(),
            "Color Nodes pending restoration or grid transfer failed");
        check(nodesToggle->isEnabled(), "Native Nodes toggle disabled while floating");
        check(unrelatedLightBox->layout()==clipsLayout && clipsContent->parentWidget()==unrelatedLightBox
            && clipsContent->window()==&main,"Legacy Clips intent detached native content");
        QVector<QWidget *> colorHosts;
        check(unrelatedLightBox->findChildren<QToolButton *>("resolvePaneUndockButton").isEmpty(),"Unsupported Clips acquired an Undock button");
        for (int i=0;i<3;++i) {
            auto host=colorControls[i]->window(); colorHosts.append(host);
            check(host->objectName()=="unboundFloating_"+colorNames[i] && host->isVisible() && colorControls[i]->isVisible(),
                "New Color pane failed pending restore/native initialization");
            check(colorActivations[i]==(restoreToggles && (i==2 || (selectedColor && i==0)) ? 2 : 1),"Shared Color sidebar activation was repeated or skipped");
            check(colorSlots[i]->layout()==colorLayouts[i],"Color sidebar native grid replaced");
            colorControls[i]->click(); check(colorEdits[i]==1,"Floating Color pane lost signals");
            check(!Unbound::PaneSettings(settingsPath).floating(colorNames[i]),"Color pane persisted without namespace");
        }
        if (restoreToggles) {
            check(!nodesToggle->isChecked() && !nodes->isVisible() && colorSidebar->isVisible()==selectedColor,"Color choices did not restore main-window space");
            for(int i=0;i<3;++i) check(colorToggles[i]->isChecked()==(selectedColor && i==0)
                && Unbound::PaneSettings(settingsPath).toggle("Color/"+colorNames[i])==(selectedColor && i==0),"Color saved state was lost during initialization");
            colorToggles[0]->click(); app.processEvents(); app.processEvents();
            colorToggles[1]->click(); app.processEvents(); app.processEvents();
            check(!Unbound::PaneSettings(settingsPath).toggle("Color/"+colorNames[0])
                && Unbound::PaneSettings(settingsPath).toggle("Color/"+colorNames[1]),"Exclusive Color selector changes were not persisted");
            colorToggles[1]->click(); app.processEvents(); app.processEvents();
            check(!Unbound::PaneSettings(settingsPath).toggle("Color/"+colorNames[1]),"Color off click not persisted");
            // Remaining tests expect Nodes selected before exercising off/on cycles.
            nodesToggle->click(); app.processEvents(); app.processEvents();
            std::puts("PASS: Color toolbar state restoration, shared sidebar exclusivity and user changes");
        }
        colorPage->hide(); page->show(); app.processEvents();
        for (auto host:colorHosts) check(!host->isVisible(),"Color pane remained visible on Edit");
        page->hide(); colorPage->show(); app.processEvents();
        for (int i=0;i<3;++i) {
            check(colorHosts[i]->isVisible(),"Color pane failed page-return restore");
            check(colorHosts[i]->close() && colorSlots[i]->layout()==colorLayouts[i]
                && colorControls[i]->parentWidget()==colorSlots[i],"Color pane did not dock into native layout");
            check(!Unbound::PaneSettings(settingsPath).floating("Color/"+colorNames[i]),"Color docking intent not saved");
            colorToggles[i]->setChecked(false); colorToggles[i]->click(); app.processEvents();
            auto button=colorSlots[i]->findChild<QToolButton *>("resolvePaneUndockButton");
            check(button && button->isVisible(),"Color pane Undock button missing");
            button->click(); app.processEvents();
            auto host=colorControls[i]->window();
            check(host!=&main && host->isVisible(),"Color pane manual repeat undock failed");
            app.processEvents(); app.processEvents();
            check(!colorToggles[i]->isChecked() && host->isVisible() && colorControls[i]->isVisible(),
                "Manual Color undock did not reclaim space while retaining content");
            colorToggles[i]->click(); app.processEvents(); app.processEvents();
            colorToggles[i]->click(); app.processEvents(); app.processEvents();
            check(host->isVisible() && colorControls[i]->isVisible(),"Native selector blanked floating Color pane");
            check(host->close(),"Color pane repeat docking failed");
        }
        std::puts("PASS: Color Gallery/LUTs/Media Pool independent restore; legacy Clips intent ignored; exclusive selectors, live controls, page visibility, docking and persistence");

        for (int toggleCycle=0; toggleCycle<4; ++toggleCycle) {
            nodesToggle->click(); app.processEvents(); app.processEvents();
            check(nodes->isVisible() == nodesToggle->isChecked(), "Nodes toolbar no longer controls main-window space");
            check(nodeControl->parentWidget() == floatingNodes && nodeControl->isVisible()
                && floatingNodes->isVisible(), "Native toggle stole or hid floating graph");
            check(nodesGrid->count() == 1 && nodesGrid->itemAt(0)->widget()->objectName() == "unboundSidebarPlaceholder",
                "Native toggle damaged retained Nodes grid/placeholder");
        }
        check(floatingEdit && !floatingEdit->isVisible(), "Edit floating pane leaked onto Color");
        int row, column, rows, columns;
        nodesGrid->getItemPosition(0, &row, &column, &rows, &columns);
        check(row == 1 && column == 2 && rows == 2 && columns == 3
            && nodesGrid->columnStretch(2) == 3 && nodesGrid->rowStretch(1) == 2, "Grid structure changed during transfer");
        nodeControl->click();
        check(nodeEdits == 1, "Floating graph lost live signals");
        colorPage->hide(); page->show(); app.processEvents();
        check(!floatingNodes->isVisible() && floatingEdit->isVisible(), "Color/Edit floating visibility not isolated");
        page->hide(); colorPage->show(); app.processEvents();
        check(floatingNodes->isVisible() && !floatingEdit->isVisible(), "Returning Color lost floating Nodes");
        check(floatingNodes->close() && nodes->layout() == nodesGrid, "Nodes grid did not dock intact");
        check(nodesToggle->isEnabled() && nodesGrid->itemAt(0)->widget() == nodeControl, "Nodes docking failed to restore control/layout");
        auto nodesButton = nodes->findChild<QToolButton *>("resolvePaneUndockButton");
        check(nodesButton && nodesButton->isVisible(), "Nodes Undock button missing after docking");
        for (int cycle = 0; cycle < 3; ++cycle) {
            if (cycle) {
                nodesToggle->click(); app.processEvents();
                check(!nodesToggle->isChecked() && nodesGrid->count() == 0, "Docked native off did not empty grid");
                nodesToggle->click(); app.processEvents();
                check(nodesToggle->isChecked() && nodesGrid->itemAt(0)->widget() == nodeControl, "Docked native on did not repopulate grid");
            }
            nodesButton->click(); app.processEvents();
            QWidget *repeatedHost = nullptr;
            for (auto host : QApplication::topLevelWidgets())
                if (host->objectName() == "unboundFloating_m_pFrameNodeGraph" && host->isVisible()) repeatedHost = host;
            check(repeatedHost != nodes && nodes->layout() == nodesGrid && nodesToggle->isEnabled(), "Repeated Nodes undock failed");
            if (cycle < 2) {
                app.processEvents(); app.processEvents();
                check(!nodes->isVisible() && nodeControl->parentWidget() == repeatedHost && nodeControl->isVisible(),
                    "Cannot reclaim Nodes space while floating");
                check(repeatedHost->close(), "Repeated Nodes dock failed");
                app.processEvents();
                check(nodeControl->parentWidget() == nodes && nodesToggle->isEnabled() && nodesToggle->isChecked()
                    && nodes->isVisible(), "Repeated Nodes dock did not show graph");
            }
        }
        check(nodeControl->parentWidget() != nodes && Unbound::PaneSettings(settingsPath).floating("Color/m_pFrameNodeGraph"),
            "Manual Nodes undocking or page-specific persistence failed");
        check(!Unbound::PaneSettings(settingsPath).floating("m_pFrameNodeGraph"), "Nodes wrote an unscoped settings key");
        colorPage->hide();
        std::puts("PASS: Color Nodes grid spans/stretch preserved, manual/pending restore, live signals and Edit/Color isolation");
        page->hide(); // Quitting from a different page must retain floating intent.
        QTimer::singleShot(0, &main, &QWidget::close);
        QTimer::singleShot(1500, [&]() { app.exit(77); });
        check(app.exec() == 0, "Standalone helper blocked application shutdown");
        Unbound::PaneSettings afterQuit(settingsPath);
        check(afterQuit.floating("frameEditPageMediaPool"), "Shutdown cleanup erased floating intent");
        check(pane->layout() == original, "Shutdown failed to restore native layout");
        check(nodes->layout() == nodesGrid && nodeControl->parentWidget() == nodes && nodesToggle->isEnabled()
            && afterQuit.floating("Color/m_pFrameNodeGraph"),
            "Nodes shutdown damaged layout or erased floating intent");
        std::puts("PASS: legacy settings migration, automatic/pending restore, explicit docking persists, hidden-page quit retains floating intent");
        std::puts("PASS: persistent placement load, debounce autosave while floating, disk recall, pane isolation");
        std::puts("PASS: standalone DLL bootstrap, duplicate-start guard, button discovery, detach/restore and natural exit");
        return 0;
    }
    QWidget window;
    auto outer = new QVBoxLayout(&window);
    auto parent = new QWidget(&window);
    outer->addWidget(parent);
    auto layout = new QHBoxLayout(parent);
    auto content = new QWidget(parent);
    content->setMinimumSize(50, 60);
    content->setMaximumSize(800, 900);
    layout->addWidget(content, 3);
    window.resize(500, 600);
    window.show();
    app.processEvents();
    const auto flags = content->windowFlags();
    Unbound::PaneFloatSession session;
    for (int i = 0; i < 3; ++i) {
        check(session.start(content).isEmpty(), "Float failed");
        check(session.active() && content->parentWidget() == session.floatingWindow(), "Incorrect floating parent");
        check(layout->count() == 1 && layout->itemAt(0)->widget() != content, "Placeholder missing");
        check(!session.start(content).isEmpty(), "Duplicate float accepted");
        session.floatingWindow()->resize(700, 700);
        app.processEvents();
        if (i == 0) check(session.restore().isEmpty(), "Explicit restoration failed");
        if (i == 1) session.floatingWindow()->close();
        if (i == 2) { parent->hide(); app.processEvents(); }
        check(!session.active(), "Session still active after restoration");
        check(content->parentWidget() == parent && layout->itemAt(0)->widget() == content, "Original parent/layout not restored");
        check(layout->stretch(0) == 3, "Layout stretch changed");
        check(content->windowFlags() == flags, "Window flags changed");
        check(content->minimumSize() == QSize(50, 60) && content->maximumSize() == QSize(800, 900), "Size constraints changed");
        parent->show(); app.processEvents();
    }
    {
        Unbound::PaneFloatSession temporary;
        check(temporary.start(content).isEmpty(), "Destructor test float failed");
    }
    check(content->parentWidget() == parent, "Session destruction did not restore pane");
    auto extra = new QWidget(parent);
    layout->addWidget(extra);
    check(!session.start(content).isEmpty(), "Unsupported layout accepted");
    check(content->parentWidget() == parent && layout->count() == 2, "Refusal modified target");
    QWidget editPage;
    editPage.resize(1000, 700);
    auto pool = new QWidget(&editPage);
    pool->setGeometry(0, 0, 300, 500);
    auto poolLayout = new QHBoxLayout(pool);
    auto poolContent = new QWidget(pool);
    poolLayout->addWidget(poolContent);
    auto viewer = new QWidget(&editPage);
    const QRect originalViewer(304, 0, 500, 500);
    viewer->setGeometry(originalViewer);
    auto divider = new QWidget(&editPage);
    divider->setGeometry(300, 0, 4, 500);
    editPage.show(); app.processEvents();
    for (int i = 0; i < 5; ++i) {
        check(session.start(poolContent, viewer, divider).isEmpty(), "Reclaim float failed");
        app.processEvents();
        check(session.active(), "Intentional slot hiding restored the session");
        check(pool->isHidden() && divider->isHidden(), "Empty slot/divider not hidden");
        check(viewer->geometry() == QRect(0, 0, 804, 500), "Viewer did not reclaim slot");
        if (i == 0) check(session.restore().isEmpty(), "Reclaim explicit restore failed");
        if (i == 1) session.floatingWindow()->close();
        if (i == 2) { editPage.hide(); app.processEvents(); }
        if (i == 3) { editPage.resize(1100, 700); app.processEvents(); }
        if (i == 4) { viewer->resize(700, 500); app.processEvents(); }
        check(!session.active(), "Reclaim restore did not complete");
        check(poolContent->parentWidget() == pool && poolLayout->itemAt(0)->widget() == poolContent, "Pool restoration failed");
        check(!pool->isHidden() && !divider->isHidden(), "Original visibility not restored");
        check(viewer->geometry() == originalViewer, "Original viewer geometry not restored");
        editPage.show(); app.processEvents();
    }
    viewer->move(400, 100);
    check(!session.start(poolContent, viewer, divider).isEmpty(), "Incompatible viewer geometry accepted");
    check(!session.active() && poolContent->parentWidget() == pool, "Geometry refusal detached content");
    QWidget sidebarPage;
    QWidget *paneSlots[5];
    QVBoxLayout *layouts[5];
    QPushButton *buttons[5];
    Unbound::SidebarFloatSession sidebars[5];
    sidebarPage.show();
    int clicks = 0;
    for (int i = 0; i < 5; ++i) {
        paneSlots[i] = new QWidget(&sidebarPage);
        layouts[i] = new QVBoxLayout(paneSlots[i]);
        buttons[i] = new QPushButton(QString::number(i), paneSlots[i]);
        QObject::connect(buttons[i], &QPushButton::clicked, [&clicks]() { ++clicks; });
        layouts[i]->addWidget(buttons[i], 3);
        auto second = new QWidget(paneSlots[i]);
        layouts[i]->addWidget(second, 1);
        if (i == 0) paneSlots[i]->show();
        else paneSlots[i]->hide();
        second->hide(); // Explicitly hidden inner controls must stay hidden.
        app.processEvents();
        check(sidebars[i].start(paneSlots[i], QString::number(i)).isEmpty(), "Sidebar float failed");
        check(sidebars[i].floatingWindow()->isWindow() && !sidebars[i].floatingWindow()->parentWidget(), "Floating pane still has a window owner");
        check(sidebars[i].floatingWindow()->layout() == layouts[i], "Complete layout not transferred");
        check(paneSlots[i]->isHidden() == (i != 0), "Float changed original slot visibility");
        check(second->isHidden(), "Float exposed explicitly hidden inner control");
        paneSlots[i]->hide(); app.processEvents();
        check(sidebars[i].active() && buttons[i]->isVisible(), "Toolbar slot hiding hid floating content");
        buttons[i]->click();
    }
    check(clicks == 5, "Floating button connections lost");
    check(sidebars[0].floatingWindow()->close(), "Floating pane rejected successful close (can veto shutdown)"); app.processEvents();
    check(!sidebars[0].active() && sidebars[1].active(), "Closing one affected other windows");
    check(paneSlots[0]->isHidden(), "Restoring switched toolbar selection");
    sidebarPage.hide(); app.processEvents();
    for (int i = 0; i < 5; ++i) {
        if (i != 0) {
            check(sidebars[i].active() && !sidebars[i].floatingWindow()->isVisible(), "Page hiding failed to hide retained sidebar");
            buttons[i]->click();
            check(sidebars[i].floatingWindow()->close(), "Close on hidden page rejected");
        }
        check(!sidebars[i].active(), "Explicit close did not restore sidebar");
        check(paneSlots[i]->layout() == layouts[i] && layouts[i]->count() == 2, "Original complete layout not restored");
        check(layouts[i]->stretch(0) == 3 && buttons[i]->parentWidget() == paneSlots[i], "Sidebar structure changed");
    }
    sidebarPage.show(); paneSlots[0]->show(); app.processEvents();
    check(buttons[0]->isVisible(), "Restored content remains hidden");
    {
        Unbound::SidebarFloatSession temporary;
        check(temporary.start(paneSlots[0], "Destructor").isEmpty(), "Sidebar repeat float failed");
    }
    check(paneSlots[0]->layout() == layouts[0], "Sidebar destructor failed to restore");
    for (int i = 0; i < 5; ++i) {
        paneSlots[i]->show();
        const auto margins = paneSlots[i]->contentsMargins();
        {
            Unbound::PaneDecoration decoration(paneSlots[i], nullptr, [&, i]() {
                if (sidebars[i].active()) check(sidebars[i].restore().isEmpty(), "Button restore failed");
                else check(sidebars[i].start(paneSlots[i], "Decorated").isEmpty(), "Button float failed");
            });
            app.processEvents();
            check(decoration.button()->isVisible(), "Undock button invisible");
            check(decoration.button()->geometry().bottom() < paneSlots[i]->contentsRect().top(), "Button covers native content");
            decoration.button()->click(); app.processEvents();
            check(sidebars[i].active(), "Undock button failed");
            decoration.setFloating(true);
            check(decoration.button()->isChecked() && decoration.button()->toolTip()=="Click to dock; drag to move", "Floating toggle state missing");
            paneSlots[i]->resize(240, 300); app.processEvents();
            check(decoration.button()->geometry().right() < paneSlots[i]->width(), "Button outside resized pane");
            auto floating = sidebars[i].floatingWindow();
            floating->setGeometry(100 + i * 10, 80 + i * 10, 360 + i * 10, 300 + i * 10);
            app.processEvents();
            const auto remembered = floating->geometry();
            check(!floating->windowTitle().contains("close to restore"), "Title still contains restore hint");
            floating->close(); app.processEvents();
            decoration.setFloating(false);
            check(decoration.button()->isVisible(), "Undock button not restored");
            decoration.button()->click(); app.processEvents();
            check(sidebars[i].floatingWindow()->geometry() == remembered, "Floating geometry was not remembered");
            check(sidebars[i].restore().isEmpty(), "Explicit geometry test restore failed");
            decoration.button()->click(); app.processEvents();
            check(sidebars[i].floatingWindow()->geometry() == remembered, "Explicit restore lost geometry");
            sidebars[i].floatingWindow()->close(); app.processEvents();
            check(!sidebars[i].active() && paneSlots[i]->layout() == layouts[i], "Restore button failed");
        }
        check(paneSlots[i]->contentsMargins() == margins, "Decoration did not restore margins");
    }
    std::puts("PASS: all five decorations: undock/close, hidden button, remembered position/size, reserved strip, margin cleanup");
    QWidget nestedPage;
    auto timeline = new QWidget(&nestedPage);
    auto mixerSlot = new QWidget(timeline);
    mixerSlot->setFixedWidth(69);
    auto mixerLayout = new QVBoxLayout(mixerSlot);
    auto mixerHeader = new QWidget(mixerSlot);
    auto mixerBody = new QWidget(mixerSlot);
    mixerLayout->addWidget(mixerHeader);
    mixerLayout->addWidget(mixerBody);
    nestedPage.show(); timeline->show(); mixerSlot->show(); app.processEvents();
    Unbound::SidebarFloatSession mixerSession;
    check(mixerSession.start(mixerSlot, "Mixer", &nestedPage).isEmpty(), "Nested Mixer float failed");
    check(mixerSession.floatingWindow()->layout()->count() == 2, "Mixer header/body not transferred together");
    timeline->hide(); app.processEvents();
    check(mixerSession.active(), "Intermediate timeline hide restored Mixer prematurely");
    nestedPage.hide(); app.processEvents();
    check(mixerSession.active() && !mixerSession.floatingWindow()->isVisible(), "Page hide failed to hide retained Mixer");
    check(mixerSession.restore().isEmpty() && mixerSlot->layout() == mixerLayout, "Nested Mixer hidden-page restore failed");
    check(mixerSlot->minimumWidth() == 69 && mixerSlot->maximumWidth() == 69, "Original Mixer constraints changed");
    std::puts("PASS: nested Mixer layout/header transfer, hidden-page explicit restoration, original width constraints");
    QWidget splitPage;
    auto splitLayout = new QVBoxLayout(&splitPage);
    auto split = new QSplitter(&splitPage);
    splitLayout->addWidget(split);
    auto timelineContent = new QWidget(split);
    auto mixer = new QWidget(split);
    auto contents = new QVBoxLayout(mixer);
    contents->addWidget(new QWidget(mixer));
    splitPage.resize(1000, 600); splitPage.show(); app.processEvents();
    split->setSizes({700, 280}); app.processEvents();
    Unbound::SidebarFloatSession reclaim;
    for (int cycle = 0; cycle < 3; ++cycle) {
        const auto originalSizes = split->sizes();
        const int timelineWidth = timelineContent->width();
        check(reclaim.start(mixer, "Mixer", &splitPage, true).isEmpty(), "Mixer reclaim failed");
        app.processEvents();
        check(mixer->isHidden() && timelineContent->width() > timelineWidth, "Timeline did not reclaim Mixer space");
        mixer->show(); app.processEvents(); app.processEvents();
        check(mixer->isHidden() && reclaim.active(), "Native toggle exposed floating Mixer slot");
        if (cycle == 0) reclaim.floatingWindow()->close();
        if (cycle == 1) check(reclaim.restore().isEmpty(), "Explicit Mixer reclaim restore failed");
        if (cycle == 2) {
            splitPage.hide(); app.processEvents();
            check(reclaim.active() && !reclaim.floatingWindow()->isVisible(), "Mixer page-context visibility incorrect");
            check(reclaim.floatingWindow()->close(), "Mixer close from hidden page failed");
        }
        app.processEvents();
        splitPage.show(); app.processEvents();
        check(!reclaim.active() && !mixer->isHidden(), "Mixer slot visibility not restored");
        check(split->sizes() == originalSizes, "Mixer splitter allocation not restored");
    }
    std::puts("PASS: Mixer releases timeline space, reclaims exact splitter allocation on close/explicit/hidden-page close");
    Unbound::PaneFloatingWindow background;
    background.resize(120, 100); background.show(); app.processEvents();
    check(background.grab().toImage().pixelColor(20, 20) == QColor(20, 20, 20), "Empty floating host is not dark");
    background.close();
    Unbound::PaneFloatingWindow deferred;
    auto deferredLayout = new QVBoxLayout(&deferred);
    auto deferredContent = new QWidget(&deferred);
    deferredLayout->addWidget(deferredContent);
    deferred.resize(400, 300); deferred.show(); app.processEvents();
    const auto initialContentSize = deferredContent->size();
    deferred.beginDeferredResize(); deferred.beginDeferredResize();
    for (int i = 0; i < 8; ++i) {
        deferred.resize(420 + i * 20, 320 + i * 10); app.processEvents();
        check(deferredContent->size() == initialContentSize, "Content resized during deferred drag");
    }
    deferred.finishDeferredResize(); app.processEvents();
    check(deferredLayout->isEnabled() && deferredContent->width() > initialContentSize.width(), "Final layout not applied");
    deferred.resize(650, 500); app.processEvents();
    check(deferredContent->width() > initialContentSize.width(), "Normal layout not resumed");
    check(reclaim.start(mixer, "Mixer", &splitPage, true).isEmpty(), "Mid-drag docking setup failed");
    auto dragHost = static_cast<Unbound::PaneFloatingWindow *>(reclaim.floatingWindow());
    dragHost->beginDeferredResize();
    check(reclaim.restore().isEmpty() && contents->isEnabled(), "Docking left layout disabled");
    std::puts("PASS: content geometry held throughout resize, final layout applied, docking re-enables layout");
    QStackedWidget pages;
    auto edit = new QWidget;
    auto color = new QWidget;
    pages.addWidget(edit); pages.addWidget(color);
    auto originalSlot = new QWidget(edit);
    auto originalLayout = new QVBoxLayout(originalSlot);
    originalLayout->addWidget(new QPushButton("Pane content", originalSlot));
    pages.show(); originalSlot->show(); app.processEvents();
    Unbound::SidebarFloatSession acrossPages;
    check(acrossPages.start(originalSlot, "Page switch", edit).isEmpty(), "Page-switch test setup failed");
    QPointer<QWidget> persistentHost = acrossPages.floatingWindow();
    persistentHost->setGeometry(100, 100, 450, 330); app.processEvents();
    const auto pageGeometry = persistentHost->geometry();
    pages.setCurrentWidget(color); app.processEvents();
    check(acrossPages.active() && !persistentHost->isVisible(), "Color page did not hide retained floating pane");
    pages.setCurrentWidget(edit); app.processEvents();
    check(acrossPages.floatingWindow() == persistentHost && persistentHost->isVisible(), "Returning to Edit did not reshow same floating pane");
    check(persistentHost->windowOpacity() == 1.0, "Returning pane should appear immediately at full opacity");
    pages.setCurrentWidget(color); pages.setCurrentWidget(edit); pages.setCurrentWidget(color);
    app.processEvents();
    check(!persistentHost->isVisible(), "Queued show leaked a window after rapid page switch");
    check(persistentHost->windowOpacity() == 1.0, "Page hide left pane transparent");
    pages.setCurrentWidget(edit); app.processEvents();
    check(persistentHost->isVisible(), "Rapid page switching lost hidden-window state");
    check(persistentHost->geometry() == pageGeometry, "Page switch changed floating geometry");
    QEventLoop fadeWait;
    QTimer::singleShot(220, &fadeWait, &QEventLoop::quit);
    fadeWait.exec();
    check(persistentHost->isVisible() && persistentHost->windowOpacity() == 1.0,
        "Returning pane did not remain fully opaque");
    pages.setCurrentWidget(color); app.processEvents();
    check(persistentHost->close() && originalSlot->layout() == originalLayout && pages.currentWidget() == color,
        "Docking from Color changed active page or failed restoration");
    pages.setCurrentWidget(edit); app.processEvents();
    check(acrossPages.start(originalSlot, "Destruction", edit).isEmpty(), "Destruction test setup failed");
    persistentHost = acrossPages.floatingWindow();
    delete originalSlot;
    QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
    check(!acrossPages.active() && !persistentHost, "Destroyed source pane left orphan floating window");
    std::puts("PASS: Edit/Color page switches preserve windows and geometry; immediate full-opacity return; hidden-page docking and source destruction clean up");
    QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
    std::puts("PASS: five simultaneous sidebar windows, live signals, independent close, hidden-slot restore, page-hide survival, full layout preservation, repeat float and destructor restore");
    std::puts("PASS: detach/restore lifecycle and constraints; viewer expansion, explicit/close/page-hide/root-resize/layout-change restoration; incompatible geometry refusal");
    return 0;
}





