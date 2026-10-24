#include "windowplacement.h"
#include "fixtures.h"
#include "mainwindowsnap.h"
#include "windowgroupactivation.h"
#include <QDialog>
#include <QWindow>
#include <QCursor>
#include <QLineEdit>
#include <QElapsedTimer>

// Model Resolve's main frame: client content reaches the outer top edge even
// though the native style still includes a caption and resize frame.
class ResolveTopEdgeFixture : public QWidget
{
public:
    bool clampTopOnMoveExit = false;
protected:
    bool nativeEvent(const QByteArray &type, void *message, long *result) override
    {
        const auto msg = static_cast<MSG *>(message);
        if (msg->message == WM_EXITSIZEMOVE && clampTopOnMoveExit) {
            RECT rect{}; GetWindowRect(msg->hwnd, &rect);
            SetWindowPos(msg->hwnd, nullptr, rect.left, rect.top + 1, 0, 0,
                SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE);
        }
        if (msg->message == WM_NCCALCSIZE && !IsZoomed(msg->hwnd)) {
            auto rect = reinterpret_cast<RECT *>(msg->lParam);
            const LONG top = rect->top;
            DefWindowProc(msg->hwnd, msg->message, msg->wParam, msg->lParam);
            rect->top = top;
            *result = 0;
            return true;
        }
        return QWidget::nativeEvent(type, message, result);
    }
};

void initializePlatformTests()
{
    SetErrorMode(SEM_FAILCRITICALERRORS | SEM_NOGPFAULTERRORBOX);
}
void settlePlatformWindowState(QWidget &) {}
std::function<unsigned long(void *)> resolveBootstrap(QLibrary &helper)
{
    auto start=reinterpret_cast<int (*)(void (*)(int))>(helper.resolve("UnboundLuaAttach"));
    if (!start) return {};
    return [start](void *argument) -> unsigned long {
        if (argument) return 0;
        // The production Lua entry queues attachment and reports completion.
        // Drain that queue here so existing pane fixtures begin after attachment.
        static int completed;
        completed=0;
        const auto accepted=start([](int result) { completed=result; });
        if (!accepted) return 0;
        QElapsedTimer deadline; deadline.start();
        while (!completed && deadline.elapsed()<3000) QCoreApplication::processEvents();
        return completed==2 || completed==3 ? 1 : 0;
    };
}
QRect nativeWindowBounds(QWidget &window)
{
    RECT rect{};
    GetWindowRect(reinterpret_cast<HWND>(window.winId()), &rect);
    return QRect(rect.left, rect.top, rect.right-rect.left, rect.bottom-rect.top);
}
void prepareNativePlacementTest(QWidget &main, Unbound::PaneSettings &settings)
{
    if (QGuiApplication::platformName()==QLatin1String("windows")) {
        // Model Resolve's mismatched Qt frame cache: the Qt rectangle is
        // deliberately wrong, while the recorded native placement is exact.
        auto record=QJsonDocument::fromJson(settings.geometry("MainWindow/WStackPage_Conform")).object();
        check(record.value("format").toString()=="unbound-main-placement-1","Native placement was not recorded");
        record.insert("qt",QString::fromLatin1(main.saveGeometry().toBase64()));
        settings.save("MainWindow/WStackPage_Conform",QJsonDocument(record).toJson(QJsonDocument::Compact));
    }
}
void verifyNativePlacementTest(QWidget &main, const QRect &expectedNative)
{
    if (QGuiApplication::platformName()==QLatin1String("windows")) {
        const auto actualNative=nativeWindowBounds(main);
        check(expectedNative==actualNative,"Native restoration lost exact rectangle or used mismatched Qt geometry");
    }
}
bool runPlatformDiscoveryTests(QApplication &app)
{
    if (app.arguments().contains("--discovery-test")) {
        QTemporaryDir directory;
        check(directory.isValid(),"Cannot create discovery settings directory");
        qputenv("UNBOUND_SETTINGS_FILE",directory.filePath("Unbound.json").toUtf8());
        QLibrary helper(app.arguments().last()); helper.setLoadHints(QLibrary::PreventUnloadHint);
        check(helper.load(),qPrintable(helper.errorString()));
        auto attach=reinterpret_cast<int (*)(void (*)(int))>(helper.resolve("UnboundLuaAttach"));
        check(attach && attach(nullptr)==0,"Helper accepted a missing completion callback");
        auto start=resolveBootstrap(helper);
        check(start(nullptr)==1,"Discovery bootstrap failed");
        app.processEvents();
        auto settle=[] { for (int i=0;i<8;++i) QCoreApplication::processEvents(); };
        // The project main window and its panes do not exist at startup.
        QWidget main; main.setObjectName("UiMainWindow"); main.show(); settle();
        auto page=new QWidget(&main); page->setObjectName("frameEditPageWidgetsContainer"); page->show();
        auto makePane=[&] {
            auto pane=new QWidget(page); pane->setObjectName("frameEditPageMediaPool");
            auto layout=new QVBoxLayout(pane); layout->addWidget(new QPushButton("Live content",pane));
            pane->show(); return pane;
        };
        auto pane=makePane(); settle();
        check(pane->findChildren<QToolButton *>("resolvePaneUndockButton").size()==1,"Late pane not discovered");
        auto toggle=new QPushButton("Native pool",page); toggle->setObjectName("btnEditToggleMediaPoolInSSM");
        toggle->setCheckable(true); toggle->setChecked(true); toggle->show(); settle();
        auto button=pane->findChild<QToolButton *>("resolvePaneUndockButton");
        button->click(); settle();
        check(button->toolTip()=="Click to dock; drag to move" && !toggle->isChecked(),"Late toggle not cached or manual detach failed");
        button->click(); settle();
        check(button->toolTip()=="Undock" && toggle->isChecked(),"Dock state not synchronized without polling");
        delete pane; delete toggle; settle();
        pane=makePane(); settle();
        check(pane->findChildren<QToolButton *>("resolvePaneUndockButton").size()==1,"Replacement pane not discovered");
        // A hidden lazy page is indexed when constructed, without selecting it.
        auto color=new QWidget(&main); color->setObjectName("WStackPage_Color"); color->hide();
        auto nodes=new QWidget(color); nodes->setObjectName("m_pFrameNodeGraph");
        settle();
        check(nodes->findChildren<QToolButton *>("resolvePaneUndockButton").isEmpty(),"Layout-less pane decorated");
        auto grid=new QGridLayout(nodes); grid->addWidget(new QPushButton("Graph",nodes)); settle();
        check(nodes->findChildren<QToolButton *>("resolvePaneUndockButton").size()==1,"Late hidden layout not discovered");
        page->hide(); color->show(); settle(); color->hide(); page->show(); settle();
        auto controller=app.findChild<QObject *>("UnboundPaneController");
        check(controller,"Controller missing");
        QEventLoop idle; QTimer::singleShot(1250,&idle,&QEventLoop::quit); idle.exec();
        for (auto timer:controller->findChildren<QTimer *>())
            check(!timer->isActive() || timer->isSingleShot(),"Recurring controller timer remains active");
        check(pane->findChildren<QToolButton *>("resolvePaneUndockButton").size()==1,"Page switching duplicated decoration");
        delete controller;
        std::puts("PASS: startup before project, late pane/toggle/layout, replacement, page switching and no recurring discovery timer");
        return true;
    }
    return false;
}
void verifyAuditionHeaderDrag(QApplication &app)
{
    QWidget main; main.setObjectName("UiMainWindow"); main.setGeometry(50,600,600,500);
    QWidget page(&main); page.setObjectName("WStackPage_Audio"); page.resize(600,500);
    QWidget slot(&page); slot.setObjectName("frameAudioPanelMediaPoolContainer"); slot.resize(500,400);
    auto layout = new QVBoxLayout(&slot);
    auto content = new QWidget(&slot); layout->addWidget(content);
    auto column = new QVBoxLayout(content);
    auto preview = new QWidget(content); preview->setObjectName("previewPlayer"); column->addWidget(preview);
    auto playerLayout = new QVBoxLayout(preview);
    auto header = new QFrame(preview); header->setObjectName("framePlayerTime"); playerLayout->addWidget(header);
    header->setAttribute(Qt::WA_NativeWindow);
    auto row = new QHBoxLayout(header);
    auto title = new QLabel("Audition clip",header); row->addWidget(title);
    row->addStretch();
    auto menu = new QPushButton("Title dropdown",header); row->addWidget(menu);
    auto waveform = new QLabel("Waveform",preview); playerLayout->addWidget(waveform);
    auto transport = new QPushButton("Play",preview); playerLayout->addWidget(transport);
    main.show(); page.show(); slot.show(); app.processEvents();
    int presses = 0;
    QObject::connect(menu, &QPushButton::pressed, [&] { ++presses; });
    auto mouse = [](QWidget *widget, QEvent::Type type, QPoint point, Qt::MouseButton changed, Qt::MouseButtons held) {
        QMouseEvent event(type, point, widget->mapToGlobal(point), changed, held, Qt::NoModifier);
        QApplication::sendEvent(widget, &event);
    };
    POINT savedCursor{}; GetCursorPos(&savedCursor);
    Unbound::SidebarFloatSession session;
    for (int pass=0; pass<2; ++pass) {
        check(session.start(&slot,"Media Pool",&page,false,false,true).isEmpty(), "Audition header setup failed");
        auto host = static_cast<Unbound::PaneFloatingWindow *>(session.floatingWindow());
        host->setGeometry(1000,500,500,400); app.processEvents();
        QCursor::setPos(title->mapToGlobal(QPoint(5,5)));
        mouse(title,QEvent::MouseButtonPress,{5,5},Qt::LeftButton,Qt::LeftButton);
        QCursor::setPos(QCursor::pos()+QPoint(QApplication::startDragDistance()+1,0));
        mouse(title,QEvent::MouseMove,{30,5},Qt::NoButton,Qt::LeftButton);
        check(host->interactiveDragActive(), "Fairlight audition title did not start a host drag");
        const auto before = nativeWindowBounds(*host);
        QCursor::setPos(QCursor::pos()+QPoint(60,20));
        mouse(title,QEvent::MouseMove,{90,25},Qt::NoButton,Qt::LeftButton);
        const auto movement = nativeWindowBounds(*host).topLeft()-before.topLeft();
        if (movement!=QPoint(60,20)) std::fprintf(stderr,"Audition drag delta: %d,%d\n",movement.x(),movement.y());
        check(nativeWindowBounds(*host).topLeft()==before.topLeft()+QPoint(60,20), "Audition title drag moved the wrong window");
        mouse(title,QEvent::MouseButtonRelease,{90,25},Qt::LeftButton,Qt::NoButton);
        check(!host->interactiveDragActive() && session.active(), "Audition header release docked the pane or retained a grab");
        mouse(menu,QEvent::MouseButtonPress,{5,5},Qt::LeftButton,Qt::LeftButton);
        check(presses==pass+1, "Audition header deferred a native dropdown press");
        mouse(menu,QEvent::MouseButtonRelease,{5,5},Qt::LeftButton,Qt::NoButton);
        for (auto control : {static_cast<QWidget *>(waveform),static_cast<QWidget *>(transport)}) {
            mouse(control,QEvent::MouseButtonPress,{5,5},Qt::LeftButton,Qt::LeftButton);
            mouse(control,QEvent::MouseMove,{40,5},Qt::NoButton,Qt::LeftButton);
            check(!host->interactiveDragActive(), "Audition content/transport became a drag surface");
            mouse(control,QEvent::MouseButtonRelease,{40,5},Qt::LeftButton,Qt::NoButton);
        }
        check(session.dock().isEmpty(), "Audition header docking failed"); app.processEvents();
        check(content->parentWidget()==&slot && preview->window()==&main, "Audition header support changed docking ownership");
    }
    SetCursorPos(savedCursor.x,savedCursor.y);
    std::puts("PASS: Fairlight audition header drag, immediate dropdowns, excluded waveform/transport and repeated docking");
}
bool runPlatformWindowTests(QApplication &app)
{
    if (app.arguments().contains("--frame-paint-native")) {
        check(QGuiApplication::platformName() == "windows", "Frame painting requires Windows");
        Unbound::PaneFloatingWindow host;
        host.setGeometry(700, 250, 420, 240);
        QWidget header(&host);
        header.setGeometry(0, 0, 420, 40);
        header.setAutoFillBackground(true);
        auto palette = header.palette();
        palette.setColor(QPalette::Window, QColor(29, 47, 39));
        header.setPalette(palette);
        host.show(); app.processEvents(); header.repaint(); DwmFlush();
        const auto handle = reinterpret_cast<HWND>(host.winId());
        auto pixel = [&](int y) {
            auto dc = GetDC(handle);
            const auto color = GetPixel(dc, 200, y);
            ReleaseDC(handle, dc);
            return color;
        };
        COLORREF before[10];
        for (int y=0;y<10;++y) {
            before[y] = pixel(y);
            check(before[y] == RGB(29,47,39), "Frame paint fixture header was not rendered");
        }
        for (bool active : {false,true,false,true}) {
            SendMessage(handle, WM_NCACTIVATE, active, 0);
            GdiFlush(); DwmFlush();
            // Inspect before any Qt repaint can conceal a native frame overdraw.
            for (int y=0;y<10;++y)
                check(pixel(y) == before[y], "Native activation painting overwrote the pane header");
        }
        std::puts("PASS: native activation/deactivation preserves header pixels before Qt repaint");
        return true;
    }
    if (app.arguments().contains("--captionless-native")) {
        check(QGuiApplication::platformName() == "windows", "Captionless fixture requires Windows");
        QWidget main;
        main.show();
        Unbound::PaneFloatingWindow host;
        host.setObjectName("unboundFloating_dragTest");
        host.setWindowTitle("Captionless pane fixture");
        host.setGeometry(600, 200, 400, 300);
        // Resolve headers may have their own native HWND; Qt must keep routing
        // this child gesture rather than requiring mouse messages on the host.
        QWidget header(&host);
        header.setAttribute(Qt::WA_NativeWindow);
        header.setGeometry(0, 0, 400, 70);
        Unbound::UndockButton button(&header);
        button.setGeometry(150, 20, 20, 20);
        button.setCheckable(true);
        button.setChecked(true);
        POINT savedCursor{}; GetCursorPos(&savedCursor);
        int clicks = 0;
        QObject::connect(&button, &QToolButton::clicked, [&] { ++clicks; });
        host.show(); app.processEvents(); DwmFlush();
        auto handle = reinterpret_cast<HWND>(host.winId());
        auto verifyFrame = [&] {
            const auto style = GetWindowLongPtr(handle, GWL_STYLE);
            check(!(style & WS_CAPTION) && (style & WS_THICKFRAME)
                && (style & WS_SYSMENU) && (style & WS_MINIMIZEBOX) && (style & WS_MAXIMIZEBOX),
                "Captionless host lost native window commands or resize frame");
            check(!(GetWindowLongPtr(handle, GWL_EXSTYLE) & WS_EX_TOOLWINDOW), "Host became a tool window");
            RECT frame{}; GetWindowRect(handle, &frame);
            POINT client{0, 0}; ClientToScreen(handle, &client);
            check(client.y - frame.top == 1, "Captionless host still has a thick top strip");
            RECT clientRect{}; GetClientRect(handle, &clientRect);
            check(qAbs(host.height() * host.devicePixelRatioF() - clientRect.bottom) <= 1,
                "Qt client height disagrees with the native frame calculation");
            const int middleX = (frame.left + frame.right) / 2;
            const int middleY = (frame.top + frame.bottom) / 2;
            const POINT points[] = {{frame.left+1,middleY},{frame.right-2,middleY},
                {middleX,frame.top},{middleX,frame.bottom-2},
                {frame.left+1,frame.top},{frame.right-2,frame.top},
                {frame.left+1,frame.bottom-2},{frame.right-2,frame.bottom-2}};
            const int hits[] = {HTLEFT,HTRIGHT,HTTOP,HTBOTTOM,HTTOPLEFT,HTTOPRIGHT,HTBOTTOMLEFT,HTBOTTOMRIGHT};
            for (int i=0;i<8;++i)
                check(SendMessage(handle, WM_NCHITTEST, 0, MAKELPARAM(points[i].x, points[i].y)) == hits[i],
                    "Captionless edge/corner resizing hit test failed");
        };
        verifyFrame();
        // Resolve's crash dump showed a dispatcher-level non-client press with
        // a null result pointer. It must pass through without starting resize.
        for(UINT message : {WM_NCHITTEST,WM_NCLBUTTONDOWN,WM_SETCURSOR}) {
            MSG dispatch{}; dispatch.hwnd=handle; dispatch.message=message;
            dispatch.wParam=HTTOPRIGHT; dispatch.lParam=MAKELPARAM(HTTOPRIGHT,WM_MOUSEMOVE);
            long result=123;
            auto filter=Unbound::PaneResizeHitFilter::shared();
            check(!filter->nativeEventFilter("windows_dispatcher_MSG",&dispatch,nullptr) &&
                !filter->nativeEventFilter("windows_dispatcher_MSG",&dispatch,&result) &&
                !filter->nativeEventFilter("windows_generic_MSG",&dispatch,nullptr) &&
                result==123 && !host.interactiveDragActive(),
                "Dispatcher/null-result pass consumed a resize message or started a gesture");
        }
        // The grab band extends into content, including native child headers.
        // Neither the frame nor the content layout needs to grow to expose it.
        const auto headerHandle=reinterpret_cast<HWND>(header.winId());
        POINT clientOrigin{}; ClientToScreen(handle,&clientOrigin);
        RECT nativeClient{}; GetClientRect(handle,&nativeClient);
        const int band=MulDiv(6,GetDpiForWindow(handle),96);
        const int corner=MulDiv(12,GetDpiForWindow(handle),96);
        const int centerX=clientOrigin.x+nativeClient.right/2;
        const int centerY=clientOrigin.y+nativeClient.bottom/2;
        auto hitAt=[&](HWND window,int x,int y) {
            return SendMessage(window,WM_NCHITTEST,0,MAKELPARAM(x,y));
        };
        check(hitAt(headerHandle,centerX,clientOrigin.y+band-1)==HTTOP,
            "Native child header did not expose the widened top resize band");
        check(hitAt(handle,clientOrigin.x+band-1,centerY)==HTLEFT &&
            hitAt(handle,clientOrigin.x+nativeClient.right-band,centerY)==HTRIGHT &&
            hitAt(handle,centerX,clientOrigin.y+nativeClient.bottom-band)==HTBOTTOM,
            "Client edge resize bands were not widened to six logical pixels");
        check(hitAt(headerHandle,clientOrigin.x+corner-1,clientOrigin.y+corner-1)==HTTOPLEFT &&
            hitAt(headerHandle,clientOrigin.x+nativeClient.right-corner,clientOrigin.y+corner-1)==HTTOPRIGHT,
            "Native child header did not expose twelve-pixel corner targets");
        check(hitAt(headerHandle,centerX,clientOrigin.y+band)==HTCLIENT,
            "Resize band intercepted header content beyond the intended width");
        SendMessage(headerHandle,WM_SETCURSOR,reinterpret_cast<WPARAM>(headerHandle),MAKELPARAM(HTTOP,WM_MOUSEMOVE));
        check(GetCursor()==LoadCursor(nullptr,IDC_SIZENS),"Native child resize hit did not show the resize cursor");
        const auto originalGeometry=host.geometry();
        SendMessage(headerHandle,WM_NCLBUTTONDOWN,HTTOP,MAKELPARAM(centerX,clientOrigin.y+band-1));
        check(host.interactiveDragActive() && GetCapture()==handle,
            "Native child edge press did not start the host's nonmodal resize");
        const auto resizeStart=host.dragStartPosition();
        host.updateInteractiveDrag(POINT{resizeStart.x,resizeStart.y-24});
        SendMessage(handle,WM_LBUTTONUP,0,0);
        app.processEvents(); DwmFlush();
        check(!host.interactiveDragActive() && host.height()>originalGeometry.height(),
            "Widened edge gesture did not resize/release the host");
        host.setGeometry(originalGeometry); app.processEvents();
        // Exercise queued input through Qt's dispatcher as well as direct
        // SendMessage delivery. The reported crash used HTTOPRIGHT (13).
        for(int cornerHit : {HTTOPLEFT,HTTOPRIGHT,HTBOTTOMLEFT,HTBOTTOMRIGHT}) {
            const bool left=cornerHit==HTTOPLEFT || cornerHit==HTBOTTOMLEFT;
            const bool top=cornerHit==HTTOPLEFT || cornerHit==HTTOPRIGHT;
            const auto destination=top ? headerHandle : handle;
            PostMessage(destination,WM_NCLBUTTONDOWN,cornerHit,0);
            app.processEvents(); app.processEvents();
            check(host.interactiveDragActive() && GetCapture()==handle,
                "Queued corner press did not start host resizing");
            const auto start=host.dragStartPosition();
            host.updateInteractiveDrag(POINT{start.x+(left ? -24 : 24),start.y+(top ? -24 : 24)});
            PostMessage(handle,WM_LBUTTONUP,0,0);
            app.processEvents(); app.processEvents();
            check(!host.interactiveDragActive() && host.width()>originalGeometry.width() &&
                host.height()>originalGeometry.height(),"Queued corner resize did not grow/release both axes");
            host.setGeometry(originalGeometry); app.processEvents();
        }
        auto mouse = [&](QEvent::Type type, QPoint local, Qt::MouseButton changed, Qt::MouseButtons held) {
            QMouseEvent event(type, local, button.mapToGlobal(local), changed, held, Qt::NoModifier);
            QApplication::sendEvent(&button, &event);
        };
        auto press = [&] { mouse(QEvent::MouseButtonPress, {10,10}, Qt::LeftButton, Qt::LeftButton); };
        auto release = [&] { mouse(QEvent::MouseButtonRelease, {10,10}, Qt::LeftButton, Qt::NoButton); };
        press();
        mouse(QEvent::MouseMove, {11,10}, Qt::NoButton, Qt::LeftButton);
        check(!host.interactiveDragActive(), "Click jitter started a move");
        release(); check(clicks == 1, "Stationary click did not toggle docking action");
        button.setChecked(true);
        auto startDrag = [&] {
            QCursor::setPos(button.mapToGlobal(QPoint(10,10)));
            press();
            mouse(QEvent::MouseMove, {10 + QApplication::startDragDistance() + 1,10}, Qt::NoButton, Qt::LeftButton);
            check(host.interactiveDragActive() && QWidget::mouseGrabber() == &button && !button.isDown(),
                "Button drag did not retain its Qt mouse grab");
        };
        startDrag();
        RECT before{}; GetWindowRect(handle, &before);
        const auto cursor = host.dragStartPosition();
        SetCursorPos(cursor.x + 80, cursor.y + 60);
        mouse(QEvent::MouseMove, {40,20}, Qt::NoButton, Qt::LeftButton);
        RECT after{}; GetWindowRect(handle, &after);
        check(after.left == before.left + 80 && after.top == before.top + 60, "Button drag did not move window");
        release(); check(clicks == 1 && !host.interactiveDragActive(), "Drag release docked or retained capture");
        startDrag();
        const auto cancelStart = nativeWindowBounds(host);
        const auto cancelCursor = host.dragStartPosition();
        SetCursorPos(cancelCursor.x + 70, cancelCursor.y + 50);
        mouse(QEvent::MouseMove, {40,20}, Qt::NoButton, Qt::LeftButton);
        QKeyEvent escape(QEvent::KeyPress, Qt::Key_Escape, Qt::NoModifier);
        QApplication::sendEvent(&header, &escape);
        release();
        check(nativeWindowBounds(host) == cancelStart && clicks == 1 && !host.interactiveDragActive(),
            "Escape did not cancel movement without docking");
        startDrag();
        QEvent lostGrab(QEvent::UngrabMouse); QApplication::sendEvent(&button, &lostGrab);
        release();
        check(!host.interactiveDragActive() && clicks == 1, "Capture loss retained drag or docked");
        startDrag(); host.hide(); app.processEvents(); release();
        check(!host.interactiveDragActive() && clicks == 1, "Page hide retained drag or docked");
        host.show(); app.processEvents();
        press(); release(); check(clicks == 2, "Click after canceled drag stopped working");
        // Docked controls must not drag their enclosing main window.
        press(); mouse(QEvent::MouseMove, {40,10}, Qt::NoButton, Qt::LeftButton);
        check(!host.interactiveDragActive(), "Unchecked dock button started a move");
        mouse(QEvent::MouseButtonRelease, {40,10}, Qt::LeftButton, Qt::NoButton);
        // Route through QWidgetWindow as well as directly to the button, so
        // Qt's child targeting/implicit grab participates in the gesture.
        button.setChecked(true);
        const auto buttonPoint = button.mapTo(&header, QPoint(10,10));
        auto windowMouse = [&](QEvent::Type type, QPoint local, Qt::MouseButton changed, Qt::MouseButtons held) {
            QMouseEvent event(type, local, header.mapToGlobal(local), changed, held, Qt::NoModifier);
            QApplication::sendEvent(header.windowHandle(), &event);
        };
        const int beforeWindow = clicks;
        windowMouse(QEvent::MouseButtonPress, buttonPoint, Qt::LeftButton, Qt::LeftButton);
        windowMouse(QEvent::MouseMove, buttonPoint + QPoint(QApplication::startDragDistance()+1,0), Qt::NoButton, Qt::LeftButton);
        check(host.interactiveDragActive(), "Qt window delivery did not start button drag");
        const auto childStart = nativeWindowBounds(host);
        const auto childCursor = host.dragStartPosition();
        SetCursorPos(childCursor.x + 90, childCursor.y + 60);
        windowMouse(QEvent::MouseMove, buttonPoint + QPoint(90,60), Qt::NoButton, Qt::LeftButton);
        check(nativeWindowBounds(host).topLeft() == childStart.topLeft() + QPoint(90,60),
            "Native-child Qt mouse updates did not move floating host");
        windowMouse(QEvent::MouseButtonRelease, buttonPoint, Qt::LeftButton, Qt::NoButton);
        check(!host.interactiveDragActive() && clicks == beforeWindow, "Window drag release triggered docking");
        windowMouse(QEvent::MouseButtonPress, buttonPoint, Qt::LeftButton, Qt::LeftButton);
        windowMouse(QEvent::MouseButtonRelease, buttonPoint, Qt::LeftButton, Qt::NoButton);
        check(clicks == beforeWindow + 1, "Window drag left Qt's button grab stuck");
        SetCursorPos(savedCursor.x, savedCursor.y);
        // Background and plain title labels share the button's movement path;
        // controls still receive their original press immediately.
        QLabel title("Media Pool", &header);
        title.setAttribute(Qt::WA_NativeWindow);
        title.setGeometry(8, 6, 115, 28); title.show();
        QToolButton nativeAction(&header);
        nativeAction.setGeometry(220, 15, 70, 25); nativeAction.show();
        int nativePresses = 0;
        QObject::connect(&nativeAction, &QToolButton::pressed, [&] { ++nativePresses; });
        QLineEdit search(&header); search.setText("Native search text");
        search.setGeometry(220, 45, 160, 24); search.show();
        QLabel selectable("Selectable", &header);
        selectable.setTextInteractionFlags(Qt::TextSelectableByMouse);
        selectable.setGeometry(8, 40, 100, 25); selectable.show();
        Unbound::PaneHeaderDrag headerDrag(&header, &header);
        // Stack-owned test helper must be destroyed before its QObject parent.
        auto headerMouse = [&](QWidget *target, QEvent::Type type, QPoint local, Qt::MouseButton changed, Qt::MouseButtons held) {
            QMouseEvent event(type, local, target->mapToGlobal(local), changed, held, Qt::NoModifier);
            QObject *receiver = target->windowHandle();
            QApplication::sendEvent(receiver ? receiver : target, &event);
        };
        QCursor::setPos(title.mapToGlobal(QPoint(10,10)));
        headerMouse(&title, QEvent::MouseButtonPress, {10,10}, Qt::LeftButton, Qt::LeftButton);
        headerMouse(&title, QEvent::MouseMove, {11,10}, Qt::NoButton, Qt::LeftButton);
        check(!host.interactiveDragActive(), "Title click jitter started movement");
        headerMouse(&title, QEvent::MouseButtonRelease, {11,10}, Qt::LeftButton, Qt::NoButton);
        auto startHeaderDrag = [&](QWidget *surface, QPoint point) {
            QCursor::setPos(surface->mapToGlobal(point));
            headerMouse(surface, QEvent::MouseButtonPress, point, Qt::LeftButton, Qt::LeftButton);
            QCursor::setPos(QCursor::pos() + QPoint(QApplication::startDragDistance()+1,0));
            headerMouse(surface, QEvent::MouseMove, point + QPoint(QApplication::startDragDistance()+1,0), Qt::NoButton, Qt::LeftButton);
            check(host.interactiveDragActive(), "Header background/title did not start movement");
        };
        for (auto surface : {static_cast<QWidget *>(&title), &header}) {
            const QPoint point = surface == &title ? QPoint(10,10) : QPoint(180,55);
            startHeaderDrag(surface, point);
            const auto start = nativeWindowBounds(host);
            QCursor::setPos(QCursor::pos() + QPoint(40,30));
            headerMouse(surface, QEvent::MouseMove, point + QPoint(50,30), Qt::NoButton, Qt::LeftButton);
            check(nativeWindowBounds(host).topLeft() == start.topLeft() + QPoint(40,30),
                "Header drag did not move host through native child delivery");
            headerMouse(surface, QEvent::MouseButtonRelease, point, Qt::LeftButton, Qt::NoButton);
            check(!host.interactiveDragActive() && clicks == beforeWindow + 1, "Header release docked or retained movement");
        }
        headerMouse(&header, QEvent::MouseButtonPress, {225,20}, Qt::LeftButton, Qt::LeftButton);
        check(nativePresses == 1, "Header drag left Qt's implicit grab stuck on the title");
        headerMouse(&header, QEvent::MouseButtonRelease, {225,20}, Qt::LeftButton, Qt::NoButton);
        for (auto control : {static_cast<QWidget *>(&nativeAction), static_cast<QWidget *>(&search), static_cast<QWidget *>(&selectable)}) {
            headerMouse(control, QEvent::MouseButtonPress, {5,10}, Qt::LeftButton, Qt::LeftButton);
            if (control == &nativeAction) check(nativePresses == 2, "Header drag delayed a native action's press");
            headerMouse(control, QEvent::MouseMove, {30,10}, Qt::NoButton, Qt::LeftButton);
            check(!host.interactiveDragActive(), "Native control or selectable text became a header drag surface");
            headerMouse(control, QEvent::MouseButtonRelease, {30,10}, Qt::LeftButton, Qt::NoButton);
        }
        const auto beforeCancel = nativeWindowBounds(host);
        startHeaderDrag(&title, {10,10});
        QCursor::setPos(QCursor::pos() + QPoint(40,30));
        headerMouse(&title, QEvent::MouseMove, {50,40}, Qt::NoButton, Qt::LeftButton);
        QKeyEvent headerEscape(QEvent::KeyPress, Qt::Key_Escape, Qt::NoModifier);
        QApplication::sendEvent(&title, &headerEscape);
        check(!host.interactiveDragActive() && nativeWindowBounds(host) == beforeCancel,
            "Escape did not restore header-drag geometry");
        startHeaderDrag(&title, {10,10}); host.hide(); app.processEvents();
        check(!host.interactiveDragActive() && !QWidget::mouseGrabber(), "Page hide retained a header drag/grab");
        host.show(); app.processEvents();
        SetCursorPos(savedCursor.x, savedCursor.y);
        host.showMaximized(); app.processEvents();
        check(host.isMaximized(), "Captionless maximization failed");
        MONITORINFO maximizedMonitor{}; maximizedMonitor.cbSize = sizeof(maximizedMonitor);
        check(GetMonitorInfo(MonitorFromWindow(handle, MONITOR_DEFAULTTONEAREST), &maximizedMonitor),
            "Maximized fixture monitor unavailable");
        POINT maximizedClient{}; ClientToScreen(handle, &maximizedClient);
        check(maximizedClient.y == maximizedMonitor.rcWork.top,
            "Thin top frame clipped maximized content above the work area");
        host.showNormal(); app.processEvents(); verifyFrame();
        const auto savedClient = host.geometry();
        const auto savedGeometry = host.saveGeometry();
        host.setGeometry(800, 350, 350, 250); app.processEvents();
        check(host.restoreGeometry(savedGeometry), "Captionless geometry restoration failed");
        app.processEvents(); verifyFrame();
        check(host.geometry() == savedClient, "Thin top frame changed saved client geometry");
        host.setGeometry(300, 0, 420, 300); app.processEvents(); DwmFlush();
        const auto topSaved = Unbound::qtGeometryBytes(Unbound::qtGeometryJson(host.saveGeometry()));
        const auto topContact = Unbound::paneContactBounds(handle);
        Unbound::PaneFloatingWindow topRestored;
        topRestored.winId();
        topRestored.restoreGeometry(topSaved); topRestored.show(); app.processEvents(); DwmFlush();
        const auto restoredTopContact = Unbound::paneContactBounds(reinterpret_cast<HWND>(topRestored.winId()));
        check(restoredTopContact == topContact, "Captionless startup restoration displaced a top-snapped pane");
        host.showMinimized(); app.processEvents();
        check(host.isMinimized(), "Captionless minimization failed");
        host.showNormal(); app.processEvents(); verifyFrame();
        std::puts("PASS: captionless frame, six-pixel client resize bands, twelve-pixel corners, native-child resize cursor/gesture, button/title/background movement, immediate native controls, cancellation, top restoration and window commands");
        verifyAuditionHeaderDrag(app);
        return true;
    }
    if (app.arguments().contains("--group-activation-native")) {
        check(QGuiApplication::platformName() == "windows", "Group activation requires the Windows platform");
        QWidget main, outsider;
        Unbound::PaneFloatingWindow first, second, hidden, minimized;
        Unbound::WindowGroupActivation group;
        group.setWindow(&main);
        main.setGeometry(80, 80, 300, 200);
        first.setGeometry(400, 80, 250, 200);
        second.setGeometry(670, 80, 250, 200);
        for (auto window : {&main, &outsider, static_cast<QWidget *>(&first), static_cast<QWidget *>(&second),
                static_cast<QWidget *>(&minimized)}) {
            window->show();
        }
        minimized.showMinimized();
        auto settle = [&] { for (int i = 0; i < 4; ++i) app.processEvents(); };
        settle();
        auto handle = [](QWidget &window) { return reinterpret_cast<HWND>(window.winId()); };
        auto activate = [](HWND window) {
            SetForegroundWindow(window);
            return GetForegroundWindow() == window;
        };
        auto rank = [](HWND target) {
            int index = 0;
            for (auto window = GetTopWindow(nullptr); window; window = GetWindow(window, GW_HWNDNEXT), ++index)
                if (window == target) return index;
            return -1;
        };
        const auto mainRect = nativeWindowBounds(main), firstRect = nativeWindowBounds(first), secondRect = nativeWindowBounds(second);
        for (auto selected : {&main, static_cast<QWidget *>(&first), static_cast<QWidget *>(&second)}) {
            SetWindowPos(handle(second), HWND_TOP, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
            SetWindowPos(handle(first), HWND_TOP, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
            SetWindowPos(handle(outsider), HWND_TOP, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
            check(activate(handle(*selected)), "Cannot activate test window");
            // Also covers activation while already foreground; no app switch is required.
            QEvent activation(QEvent::WindowActivate); app.sendEvent(selected, &activation);
            settle();
            check(GetForegroundWindow() == handle(*selected), "Group activation stole selected window focus");
            for (auto peer : {&main, static_cast<QWidget *>(&first), static_cast<QWidget *>(&second)})
                check(rank(handle(*peer)) < rank(handle(outsider)), "Group member remained behind unrelated window");
            if (selected == &main) check(rank(handle(first)) < rank(handle(second)), "Peer relative order changed");
            check(!hidden.isVisible() && minimized.isMinimized(), "Group activation exposed hidden/minimized pane");
            check(nativeWindowBounds(main) == mainRect && nativeWindowBounds(first) == firstRect
                && nativeWindowBounds(second) == secondRect, "Group activation changed geometry");
            for (auto peer : {&main, static_cast<QWidget *>(&first), static_cast<QWidget *>(&second)})
                check(!(GetWindowLongPtr(handle(*peer), GWL_EXSTYLE) & WS_EX_TOPMOST), "Group became always-on-top");
        }
        QEvent activation(QEvent::WindowActivate);
        app.sendEvent(&first, &activation);
        check(activate(handle(outsider)), "Cannot activate unrelated test window");
        settle();
        check(GetForegroundWindow() == handle(outsider) && rank(handle(outsider)) < rank(handle(first)),
            "Stale activation pulled group over another window");
        QDialog modal(&main); modal.setModal(true); modal.show(); settle();
        const auto modalForeground = GetForegroundWindow();
        app.sendEvent(&main, &activation); settle();
        check(GetForegroundWindow() == modalForeground && rank(handle(modal)) < rank(handle(main)),
            "Group activation covered or stole focus from modal dialog");
        modal.hide();
        group.setWindow(nullptr);
        std::puts("PASS: native group activation from main/each pane, selected focus, peer order, geometry, hidden/minimized exclusion, stale activation and modal dialog");
        return true;
    }
    if (app.arguments().contains("--grid-native")) {
        Unbound::PaneFloatingWindow pane;
        pane.setGeometry(301,251,401,303);pane.show();pane.activateWindow();app.processEvents();
        const auto hwnd=reinterpret_cast<HWND>(pane.winId());
        const auto foregroundThread=GetWindowThreadProcessId(GetForegroundWindow(),nullptr);
        const auto thread=GetCurrentThreadId();
        const bool attached=foregroundThread!=thread && AttachThreadInput(thread,foregroundThread,TRUE);
        SetForegroundWindow(hwnd);SetFocus(hwnd);
        if(attached)AttachThreadInput(thread,foregroundThread,FALSE);
        app.processEvents();
        check(GetForegroundWindow()==hwnd,"Grid fixture could not acquire foreground input");
        const auto original=Unbound::paneContactBounds(hwnd);
        MONITORINFO monitor{};monitor.cbSize=sizeof(monitor);
        POINT cursor{};GetCursorPos(&cursor);
        GetMonitorInfo(MonitorFromPoint(cursor,MONITOR_DEFAULTTONEAREST),&monitor);
        auto wait=[] {QEventLoop loop;QTimer::singleShot(160,&loop,&QEventLoop::quit);loop.exec();};
        auto key=[](WORD code,bool down) {INPUT input{};input.type=INPUT_KEYBOARD;input.ki.wVk=code;
            input.ki.dwFlags=down?0:KEYEVENTF_KEYUP;SendInput(1,&input,sizeof(input));};
        auto guide=[] {for(auto w:QApplication::topLevelWidgets())if(w->objectName()=="unboundGridGuide" && w->isVisible())return w;return static_cast<QWidget *>(nullptr);};
        const bool began=pane.beginButtonMove();
        const auto start=pane.dragStartPosition();pane.updateInteractiveDrag({start.x+13,start.y+17});wait();
        const auto raw=Unbound::paneContactBounds(hwnd);
        key(VK_SHIFT,true);wait();
        const auto snapped=Unbound::paneContactBounds(hwnd);
        const int spacing=MulDiv(Unbound::PaneGridSpacing,GetDpiForWindow(hwnd),96);
        const bool aligned=(snapped.x()-monitor.rcWork.left)%spacing==0 && (snapped.y()-monitor.rcWork.top)%spacing==0;
        auto overlay=guide();
        const bool passive=overlay && overlay->windowFlags().testFlag(Qt::WindowTransparentForInput)
            && overlay->testAttribute(Qt::WA_ShowWithoutActivating) && GetForegroundWindow()==hwnd;
        key(VK_MENU,true);wait();
        const bool bypass=Unbound::paneContactBounds(hwnd)==raw && !guide();
        key(VK_MENU,false);wait();
        const bool resumed=guide()!=nullptr;
        key(VK_SHIFT,false);wait();
        const bool released=Unbound::paneContactBounds(hwnd)==raw && !guide();
        pane.finishButtonMove();
        // Always release physical modifiers before any assertion can exit.
        key(VK_MENU,false);key(VK_SHIFT,false);wait();
        check(began && original.size()==snapped.size(),"Grid move changed window size");
        check(aligned,"Native Shift grid did not align contact edges without pointer motion");
        check(passive,"Grid guide stole focus or accepted input");
        check(bypass && resumed,"Alt did not bypass/resume Shift grid");
        check(released,"Shift release did not restore normal snapping without pointer motion");
        std::puts("PASS: native Shift grid, stationary modifier switching, Alt precedence, passive guide and cleanup");
        return true;
    }
    if (app.arguments().contains("--snap-native")) {
        check(QGuiApplication::platformName() == "windows", "Snapping requires the Windows platform");
        ResolveTopEdgeFixture main;
        main.setObjectName("UiMainWindow");
        main.setGeometry(100, 100, 400, 300);
        Unbound::PaneFloatingWindow peer;
        peer.setObjectName("unboundFloating_snapTest");
        peer.setGeometry(950, 120, 250, 300);
        main.show(); peer.show(); app.processEvents();
        const auto handle = reinterpret_cast<HWND>(main.winId());
        const auto peerHandle = reinterpret_cast<HWND>(peer.winId());
        DwmFlush();
        RECT original;
        GetWindowRect(handle, &original);
        auto contact = Unbound::paneContactBounds(handle);
        auto other = Unbound::paneContactBounds(peerHandle);
        // Check contact bounds against DWM independently of the snap algorithm:
        // exact contact alignment alone would also pass with the wrong inset.
        const auto os = QOperatingSystemVersion::current();
        for (auto window : {handle, peerHandle}) {
            RECT frame{};
            check(SUCCEEDED(DwmGetWindowAttribute(window, DWMWA_EXTENDED_FRAME_BOUNDS,
                &frame, sizeof(frame))), "DWM frame unavailable for snap test");
            const auto actual = Unbound::paneContactBounds(window);
            if (os.majorVersion() == 10 && os.minorVersion() == 0
                && os.microVersion() >= 10240 && os.microVersion() < 22000) {
                const int expectedTop = window == handle ? 0 : 1;
                check(actual == QRect(frame.left + 1, frame.top + expectedTop,
                    frame.right - frame.left - 2, frame.bottom - frame.top - expectedTop - 1),
                    "Windows 10 contact bounds did not exclude the one-pixel translucent outline");
            } else if (os.majorVersion() == 10 && os.microVersion() >= 22000) {
                UINT border = 0;
                check(SUCCEEDED(DwmGetWindowAttribute(window, DWMWA_VISIBLE_FRAME_BORDER_THICKNESS,
                    &border, sizeof(border))), "Windows 11 border unavailable for snap test");
                POINT origin{};
                RECT client{};
                check(GetClientRect(window, &client) && ClientToScreen(window, &origin),
                    "Windows 11 client bounds unavailable for snap test");
                const int thickness = int(qMin(border, UINT(4)));
                const QRect interior(frame.left + thickness, frame.top + thickness,
                    frame.right - frame.left - 2 * thickness, frame.bottom - frame.top - 2 * thickness);
                const int contentTop = qMax(int(origin.y), int(frame.top) + thickness);
                const QRect content(origin.x, contentTop, client.right, origin.y + client.bottom - contentTop);
                check(actual == interior.united(content).intersected(QRect(frame.left, frame.top,
                    frame.right - frame.left, frame.bottom - frame.top)),
                    "Windows 11 snap bounds retained an outline or overlapped pane content");
            }
        }
        QJsonArray signature;
        for(auto screen:QGuiApplication::screens()) {
            const auto rect=screen->availableGeometry();
            signature.append(QJsonArray{screen->name(),rect.x(),rect.y(),rect.width(),rect.height(),screen->devicePixelRatio()});
        }
        auto referenceSaved=Unbound::placementJson(Unbound::Platform::saveWindowPlacement(&main,signature));
        auto falseFallback=referenceSaved.value("qtFallback").toObject();
        falseFallback.insert("clientRect",Unbound::rectangleJson(QRect(10,10,100,100)));
        referenceSaved.insert("qtFallback",falseFallback);
        const auto preview=Unbound::Platform::windowPlacementReference(&main,referenceSaved);
        auto screen=main.screen();const auto scale=screen->devicePixelRatio();
        MONITORINFO sourceMonitor{};sourceMonitor.cbSize=sizeof(sourceMonitor);
        GetMonitorInfo(MonitorFromWindow(handle,MONITOR_DEFAULTTONEAREST),&sourceMonitor);
        const QRect expected(screen->geometry().x()+qRound((contact.x()-sourceMonitor.rcMonitor.left)/scale),
            screen->geometry().y()+qRound((contact.y()-sourceMonitor.rcMonitor.top)/scale),
            qRound(contact.width()/scale),qRound(contact.height()/scale));
        check(preview==expected,"Main reference outline ignored native placement or contact compensation");
        Unbound::MainWindowSnap filter;
        filter.setWindow(&main);
        SendMessage(handle, WM_ENTERSIZEMOVE, 0, 0);
        RECT moving = original;
        OffsetRect(&moving, other.x() - contact.x() - contact.width() + 6, 0);
        check(SendMessage(handle, WM_MOVING, 0, reinterpret_cast<LPARAM>(&moving)) == TRUE, "Main native move message not handled");
        const int insetRight = original.right - contact.x() - contact.width();
        check(moving.right - insetRight == other.x(), "Main movement did not join contact edges exactly");
        RECT sizing = original;
        sizing.right = other.x() + insetRight + 6;
        check(SendMessage(handle, WM_SIZING, WMSZ_RIGHT, reinterpret_cast<LPARAM>(&sizing)) == TRUE,
            "Main native sizing message not handled");
        check(sizing.left == original.left && sizing.right - insetRight == other.x(), "Main resize snapped wrong edge or left a gap");
        SendMessage(handle, WM_EXITSIZEMOVE, 0, 0);
        filter.setWindow(nullptr);
        Unbound::PaneSnapWindows snap;
        snap.begin(handle);
        MINMAXINFO limits{};
        RECT bypass = original;
        bypass.right = other.x() + insetRight + 6;
        const auto unchanged = bypass;
        snap.apply(bypass, HTRIGHT, limits, true);
        check(bypass.right == unchanged.right, "Explicit free-drag bypass snapped");
        peer.hide(); app.processEvents();
        snap.apply(bypass, HTRIGHT, limits);
        check(bypass.right == unchanged.right, "Hidden target participated in snapping");
        peer.show(); app.processEvents(); DwmFlush();
        // Exercise the actual nonmodal floating resize path against main.
        const auto peerBounds = Unbound::paneContactBounds(peerHandle);
        RECT peerRect;
        GetWindowRect(peerHandle, &peerRect);
        SendMessage(peerHandle, WM_NCLBUTTONDOWN, HTLEFT, 0);
        const auto cursor = peer.dragStartPosition();
        const int desiredLeft = contact.x() + contact.width() - (peerBounds.x() - peerRect.left) + 6;
        peer.updateInteractiveDrag(POINT{cursor.x + desiredLeft - peerRect.left, cursor.y});
        // Hold the pointer still: transition frames must continue without new
        // mouse messages, then stop at exact native contact before release.
        QEventLoop animationWait;
        QTimer::singleShot(140,&animationWait,&QEventLoop::quit);
        animationWait.exec();
        DwmFlush();
        check(Unbound::paneContactBounds(peerHandle).x() == contact.x() + contact.width(),
            "Floating snap animation did not settle while the pointer was stationary");
        SendMessage(peerHandle, WM_LBUTTONUP, 0, 0);
        DwmFlush();
        check(Unbound::paneContactBounds(peerHandle).x() == contact.x() + contact.width(),
            "Floating resize path did not snap to main without a seam");
        if (app.arguments().contains("--restore-probe")) {
            app.processEvents();
            const auto saved=peer.saveGeometry();
            const auto originalContact=Unbound::paneContactBounds(peerHandle);
            peer.hide();
            Unbound::PaneFloatingWindow restored;
            restored.winId();
            restored.restoreGeometry(saved); restored.show(); app.processEvents(); DwmFlush();
            const auto restoredContact=Unbound::paneContactBounds(reinterpret_cast<HWND>(restored.winId()));
            std::fprintf(stderr,"Saved contact %d %d %d %d; restored %d %d %d %d\n",
                originalContact.x(),originalContact.y(),originalContact.width(),originalContact.height(),
                restoredContact.x(),restoredContact.y(),restoredContact.width(),restoredContact.height());
        }
        MONITORINFO monitor{}; monitor.cbSize = sizeof(monitor);
        check(GetMonitorInfo(MonitorFromWindow(handle, MONITOR_DEFAULTTONEAREST), &monitor),
            "Snap fixture monitor unavailable");
        const auto mainContactBefore = Unbound::paneContactBounds(handle);
        RECT mainRectBefore{}; GetWindowRect(handle, &mainRectBefore);
        const int mainTopInset = mainContactBefore.top() - mainRectBefore.top;
        SetWindowPos(handle, nullptr, monitor.rcWork.left + 400, monitor.rcWork.top - mainTopInset,
            400, 300, SWP_NOZORDER | SWP_NOACTIVATE);
        app.processEvents(); DwmFlush();
        check(Unbound::paneContactBounds(handle).top() == monitor.rcWork.top,
            "Resolve-like main visible content did not reach the work-area top");
        if (mainTopInset > 0) {
            filter.setWindow(&main);
            SendMessage(handle, WM_ENTERSIZEMOVE, 0, 0);
            RECT proposal{}; GetWindowRect(handle, &proposal);
            SendMessage(handle, WM_MOVING, 0, reinterpret_cast<LPARAM>(&proposal));
            SetWindowPos(handle, nullptr, proposal.left, proposal.top, 0, 0,
                SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE);
            main.clampTopOnMoveExit = true;
            SendMessage(handle, WM_EXITSIZEMOVE, 0, 0);
            main.clampTopOnMoveExit = false;
            app.processEvents(); DwmFlush();
            check(Unbound::paneContactBounds(handle).top() == monitor.rcWork.top,
                "Move-loop release displaced the main window from its negotiated top snap");
            SendMessage(handle, WM_ENTERSIZEMOVE, 0, 0);
            GetWindowRect(handle, &proposal);
            SendMessage(handle, WM_MOVING, 0, reinterpret_cast<LPARAM>(&proposal));
            SendMessage(handle, WM_CANCELMODE, 0, 0);
            main.clampTopOnMoveExit = true;
            SendMessage(handle, WM_EXITSIZEMOVE, 0, 0);
            main.clampTopOnMoveExit = false;
            app.processEvents(); DwmFlush();
            check(Unbound::paneContactBounds(handle).top() == monitor.rcWork.top + 1,
                "Canceled movement replayed a stale snap rectangle");
            SetWindowPos(handle, nullptr, monitor.rcWork.left + 400, monitor.rcWork.top - mainTopInset,
                0, 0, SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE);
            filter.setWindow(nullptr);
        }
        GetWindowRect(peerHandle, &peerRect);
        const auto peerContact = Unbound::paneContactBounds(peerHandle);
        const int topInset = peerContact.top() - peerRect.top;
        snap.begin(peerHandle);
        for (int delta = -3; delta <= 3; ++delta) {
            RECT proposal = peerRect;
            OffsetRect(&proposal, monitor.rcWork.left + 450 - proposal.left,
                monitor.rcWork.top - topInset + delta - proposal.top);
            snap.apply(proposal, HTCAPTION, limits);
            check(proposal.top + topInset == monitor.rcWork.top,
                "Window and desktop top edges produced a one-pixel double snap");
        }
        // Viewers retain their native Qt ancestry. Membership must come from
        // the floating host, not a pane-specific object-name convention.
        QWidget viewerPage(&main);
        Unbound::PaneFloatingWindow viewer;
        viewer.setParent(&viewerPage,viewer.windowFlags());
        viewer.setObjectName("unboundViewer_widgetEditTimelinePlayer");
        viewer.setGeometry(1200,650,350,240);
        peer.setGeometry(700,650,250,240);
        viewer.show(); peer.show(); app.processEvents(); DwmFlush();
        const auto viewerHandle=reinterpret_cast<HWND>(viewer.winId());
        for(auto movingHandle : {handle,peerHandle,viewerHandle}) {
            const auto targetHandle=movingHandle==viewerHandle ? peerHandle : viewerHandle;
            const auto targetContact=Unbound::paneContactBounds(targetHandle);
            const auto movingContact=Unbound::paneContactBounds(movingHandle);
            RECT movingRect{}; GetWindowRect(movingHandle,&movingRect);
            const int leftInset=movingContact.left()-movingRect.left;
            const int upperInset=movingContact.top()-movingRect.top;
            const int rightInset=movingRect.right-movingContact.x()-movingContact.width();
            const int lowerInset=movingRect.bottom-movingContact.y()-movingContact.height();
            snap.begin(movingHandle);
            for(int delta=-4;delta<=4;++delta) {
                RECT proposal=movingRect;
                OffsetRect(&proposal,targetContact.x()+targetContact.width()-movingContact.x()+delta,
                    targetContact.y()-movingContact.y());
                snap.apply(proposal,HTCAPTION,limits);
                check(proposal.left+leftInset==targetContact.x()+targetContact.width(),
                    "Viewer/peer horizontal snapping missed contact or produced a nearby false snap");
                proposal=movingRect;
                OffsetRect(&proposal,targetContact.x()-movingContact.x(),
                    targetContact.y()+targetContact.height()-movingContact.y()+delta);
                snap.apply(proposal,HTCAPTION,limits);
                check(proposal.top+upperInset==targetContact.y()+targetContact.height(),
                    "Viewer/peer vertical snapping missed contact or produced a nearby false snap");
            }
            RECT distant=movingRect;
            // Probe beyond the former 18-DIP zone but inside the new 24-DIP
            // zone, using the moving window's actual DPI.
            const int widenedDelta=MulDiv(21,GetDpiForWindow(movingHandle),96);
            OffsetRect(&distant,targetContact.x()+1000-movingContact.x(),
                targetContact.y()+targetContact.height()-movingContact.y()-movingContact.height()+widenedDelta);
            snap.apply(distant,HTCAPTION,limits);
            check(distant.bottom-lowerInset==targetContact.y()+targetContact.height(),
                "Distant native bottom-edge alignment failed for main/pane/viewer");
            distant=movingRect;
            OffsetRect(&distant,targetContact.x()+targetContact.width()-movingContact.x()-movingContact.width()+5,
                targetContact.y()+1200-movingContact.y());
            snap.apply(distant,HTCAPTION,limits);
            check(distant.right-rightInset==targetContact.x()+targetContact.width(),
                "Distant native right-edge alignment failed for main/pane/viewer");
            distant=movingRect;
            OffsetRect(&distant,targetContact.x()+1000-movingContact.x(),0);
            distant.bottom=targetContact.y()+targetContact.height()+lowerInset+5;
            snap.apply(distant,HTBOTTOM,limits);
            check(distant.bottom-lowerInset==targetContact.y()+targetContact.height() && distant.top==movingRect.top,
                "Distant native resize alignment failed or moved its stationary edge");
        }
        std::puts("PASS: native main move/resize routing, floating resize, exact contact alignment, bypass and hidden-window exclusion; adjacent and distant main/pane/viewer targets");
        return true;
    }
    if (app.arguments().contains("--audition-taskbar-native")) {
        check(QGuiApplication::platformName()==QLatin1String("windows"),"Taskbar fixture requires native Windows backend");
        QWidget main;main.setObjectName("UiMainWindow");main.resize(360,260);
        QWidget page(&main);page.setObjectName("WStackPage_Audio");page.resize(340,240);
        QWidget slot(&page);slot.setObjectName("frameAudioPanelMediaPoolContainer");slot.resize(320,220);
        auto layout=new QVBoxLayout(&slot);
        auto preview=new QWidget(&slot);preview->setObjectName("previewPlayer");layout->addWidget(preview);
        const auto mainHandle=reinterpret_cast<HWND>(main.winId());
        IPropertyStore *properties=nullptr;
        check(SUCCEEDED(SHGetPropertyStoreForWindow(mainHandle,IID_PPV_ARGS(&properties))),"Source property store unavailable");
        PROPVARIANT id;PropVariantInit(&id);id.vt=VT_LPWSTR;
        id.pwszVal=const_cast<wchar_t *>(L"Unbound.AuditionTaskbarTest");
        check(SUCCEEDED(properties->SetValue(PKEY_AppUserModel_ID,id)),"Setting audition test identity failed");
        properties->Release();
        main.setAttribute(Qt::WA_ShowWithoutActivating);main.show();page.show();slot.show();app.processEvents();
        Unbound::SidebarFloatSession session;
        for (bool automatic : {false,true}) {
            check(session.start(&slot,"Audition taskbar fixture",&page,false,automatic).isEmpty(),"Native audition detach failed");
            auto host=session.floatingWindow();
            const auto handle=reinterpret_cast<HWND>(host->winId());
            auto verify=[&] {
                check(host->parentWidget()==&slot && preview->window()==host,"Audition Qt ancestry changed");
                check(GetWindow(handle,GW_OWNER)==mainHandle,"Native audition owner changed");
                const auto style=GetWindowLongPtr(handle,GWL_EXSTYLE);
                check((style & WS_EX_APPWINDOW) && !(style & (WS_EX_TOOLWINDOW | WS_EX_NOACTIVATE)),"Owned audition host is excluded from shell switching");
                check(SUCCEEDED(SHGetPropertyStoreForWindow(handle,IID_PPV_ARGS(&properties))),"Audition property store unavailable");
                PROPVARIANT result;PropVariantInit(&result);
                check(SUCCEEDED(properties->GetValue(PKEY_AppUserModel_ID,&result)) && result.vt==VT_LPWSTR
                    && wcscmp(result.pwszVal,L"Unbound.AuditionTaskbarTest")==0,"Audition grouping identity lost");
                PropVariantClear(&result);properties->Release();
            };
            verify();app.processEvents();verify();
            slot.hide();verify(); // Native pane toggle reclaims main-window space.
            page.hide();app.processEvents();
            check(!host->isVisible(),"Leaving page did not hide audition host");
            page.show();app.processEvents();verify();
            check(host->isVisible(),"Returning page did not restore audition host");
            host->showMinimized();app.processEvents();verify();
            host->showNormal();app.processEvents();verify();
            check(session.dock().isEmpty() && preview->window()==&main,"Native audition docking failed");
            slot.show();app.processEvents();
        }
        std::puts("PASS: owned audition host has taskbar/Alt+Tab style and copied grouping identity; Qt/native ownership, manual/startup detach, page return, minimize/restore and docking preserved");
        return true;
    }
    if (app.arguments().contains("--taskbar-native")) {
        QWidget source;
        IPropertyStore *properties = nullptr;
        check(SUCCEEDED(SHGetPropertyStoreForWindow(reinterpret_cast<HWND>(source.winId()), IID_PPV_ARGS(&properties))), "Source property store unavailable");
        PROPVARIANT id;
        PropVariantInit(&id);
        id.vt = VT_LPWSTR;
        id.pwszVal = const_cast<wchar_t *>(L"ResolvePane.NativeTest");
        check(SUCCEEDED(properties->SetValue(PKEY_AppUserModel_ID, id)), "Setting test identity failed");
        properties->Release();
        Unbound::PaneFloatingWindow host;
        auto dragLayout = new QVBoxLayout(&host);
        dragLayout->addWidget(new QWidget(&host));
        Unbound::inheritPaneTaskbarIdentity(&host, &source);
        auto handle = reinterpret_cast<HWND>(host.winId());
        check(GetWindow(handle, GW_OWNER) == nullptr, "Native pane has an owner");
        check(!(GetWindowLongPtr(handle, GWL_EXSTYLE) & WS_EX_TOOLWINDOW), "Native pane is a tool window");
        check(SUCCEEDED(SHGetPropertyStoreForWindow(handle, IID_PPV_ARGS(&properties))), "Destination property store unavailable");
        PROPVARIANT result;
        PropVariantInit(&result);
        check(SUCCEEDED(properties->GetValue(PKEY_AppUserModel_ID, &result)) && result.vt == VT_LPWSTR
            && wcscmp(result.pwszVal, L"ResolvePane.NativeTest") == 0, "Window taskbar grouping identity not copied");
        PropVariantClear(&result);
        properties->Release();
        check(Unbound::applyPaneDarkCaption(&host, &source), "Windows rejected dark caption colors");
        RECT bounds{0, 0, 500, 400};
        SendMessage(handle, WM_SIZING, WMSZ_RIGHT, reinterpret_cast<LPARAM>(&bounds));
        check(!dragLayout->isEnabled(), "Native sizing did not defer layout");
        SendMessage(handle, WM_EXITSIZEMOVE, 0, 0);
        check(dragLayout->isEnabled(), "Native exit did not resume layout");
        POINT cursor;
        GetCursorPos(&cursor);
        SendMessage(handle, WM_NCLBUTTONDOWN, HTRIGHT, 0);
        check(host.interactiveDragActive() && !dragLayout->isEnabled(), "Custom resize did not capture input");
        RECT before, after;
        GetWindowRect(handle, &before);
        cursor = host.dragStartPosition();
        host.updateInteractiveDrag(POINT{cursor.x + 80, cursor.y});
        GetWindowRect(handle, &after);
        check(after.right > before.right && after.left == before.left, "Custom right-edge resize failed");
        SendMessage(handle, WM_LBUTTONUP, 0, 0);
        check(!host.interactiveDragActive() && dragLayout->isEnabled() && GetCapture() != handle, "Custom resize release failed");
        GetCursorPos(&cursor);
        SendMessage(handle, WM_NCLBUTTONDOWN, HTCAPTION, 0);
        check(host.interactiveDragActive() && dragLayout->isEnabled(), "Moving unnecessarily deferred content");
        GetWindowRect(handle, &before);
        cursor = host.dragStartPosition();
        host.updateInteractiveDrag(POINT{cursor.x + 40, cursor.y + 20});
        GetWindowRect(handle, &after);
        check(after.left == before.left + 40 && after.top == before.top + 20, "Custom move failed");
        SendMessage(handle, WM_KEYDOWN, VK_ESCAPE, 0);
        GetWindowRect(handle, &after);
        check(!host.interactiveDragActive() && after.left == before.left && after.top == before.top, "Escape did not cancel movement");
        SendMessage(handle, WM_NCLBUTTONDOWN, HTBOTTOMRIGHT, 0);
        ReleaseCapture();
        check(!host.interactiveDragActive() && dragLayout->isEnabled(), "Capture loss left resize active");
        std::puts("PASS: nonmodal move/resize, mouse release, Escape cancellation and capture-loss cleanup");
        std::puts("PASS: native resize message routing");
        std::puts("PASS: dark caption colors accepted; native unowned window, taskbar-compatible style, explicit AppUserModelID copied");
        return true;
    }
    return false;
}
