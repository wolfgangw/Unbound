// SPDX-License-Identifier: MIT
#include "panedecoration.h"
#include "layoutadapter.h"
void testCommandFeedback() {
    using LayoutTest::require;
    const auto wait=[](int ms) {QEventLoop loop;QTimer::singleShot(ms,&loop,&QEventLoop::quit);loop.exec();};
    QWidget main;main.setGeometry(30,30,240,120);
    Unbound::UndockButton button(&main);button.setGeometry(10,10,28,28);
    main.show();LayoutTest::activate(main);LayoutTest::settle();
    button.pulseFeedback(true);
    require(button.feedbackAnimating(),"Success pulse did not start");
    wait(1500);
    require(!button.feedbackAnimating(),"Feedback pulse repeated");
    button.pulseFeedback(false);
    require(button.feedbackAnimating(),"Failure pulse did not start");
    button.hide();
    require(!button.feedbackAnimating(),"Hidden button retained feedback");
    button.pulseFeedback(true);
    require(!button.feedbackAnimating(),"Hidden button started feedback");
    button.show();button.setDockingFailure("No docking space");
    button.pulseFeedback(true);
    require(!button.feedbackAnimating() && button.dockingWarningPending(),"Success replaced docking warning");
    button.acknowledgeDockingFailure();button.pulseFeedback(true);
    require(!button.feedbackAnimating(),"Success replaced acknowledged docking warning");
    std::puts("PASS: single feedback pulse, hidden-button exclusion and docking-warning priority");
}
