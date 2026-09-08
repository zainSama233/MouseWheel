#include <QtTest>
#include <QPushButton>
#include <QToolBar>
#include <QAction>
#include <QComboBox>
#include <QAbstractItemView>
#include <QScreen>
#include <QInputDialog>
#include <QDialogButtonBox>
#include <QTimer>
#include <QScopeGuard>
#include <Windows.h>
#include "tools/screen_annotation_session.h"
#include "tools/screenshot_session.h"
using namespace wheel;
class ScreenAnnotationTests final : public QObject {
    Q_OBJECT
    static void nativeClick(QWidget* widget,QPoint point) {
        auto* window=widget->window(); const auto local=widget->mapTo(window,point)*window->devicePixelRatioF();
        POINT native{local.x(),local.y()}; ClientToScreen(reinterpret_cast<HWND>(window->winId()),&native);
        SetCursorPos(native.x,native.y);
        INPUT event{}; event.type=INPUT_MOUSE; event.mi.dwFlags=MOUSEEVENTF_LEFTDOWN;
        QCOMPARE(SendInput(1,&event,sizeof(INPUT)),UINT(1)); QTest::qWait(60);
        event.mi.dwFlags=MOUSEEVENTF_LEFTUP;
        QCOMPARE(SendInput(1,&event,sizeof(INPUT)),UINT(1)); QTest::qWait(60);
    }
private Q_SLOTS:
    void textInputLifecycle_data() {
        QTest::addColumn<QString>("finish");
        for(const auto& name:{"accept","cancel","escape","desktop","stop"}) QTest::newRow(name)<<QString(name);
    }
    void textInputLifecycle() {
        QFETCH(QString,finish);
        POINT original{}; GetCursorPos(&original);
        const auto restore=qScopeGuard([&]{SetCursorPos(original.x,original.y);});
        ScreenAnnotationSession session; session.start(Theme::Dark);
        QWidget* overlay=nullptr; QToolBar* toolbar=nullptr;
        for(auto* w:QApplication::topLevelWidgets()) {
            if(w->objectName()=="screen-annotation-overlay" && w->screen()==QApplication::primaryScreen()) overlay=w;
            if(w->objectName()=="screen-annotation-toolbar") toolbar=qobject_cast<QToolBar*>(w);
        }
        QVERIFY(overlay && toolbar); QVERIFY(QTest::qWaitForWindowExposed(toolbar));
        for(auto* a:toolbar->actions()) if(a->text()==QStringLiteral("文字")) {
            auto* button=toolbar->widgetForAction(a); nativeClick(button,button->rect().center());
        }
        bool openedOnPress=false;
        QTimer watchdog; watchdog.setSingleShot(true);
        connect(&watchdog,&QTimer::timeout,this,[&]{
            for(auto* w:QApplication::topLevelWidgets()) if(auto* dialog=qobject_cast<QInputDialog*>(w)) {
                openedOnPress=true; dialog->reject();
            }
        });
        watchdog.start(500);
        const auto point=QPoint(400,400)*overlay->devicePixelRatioF();
        POINT native{point.x(),point.y()}; ClientToScreen(reinterpret_cast<HWND>(overlay->winId()),&native);
        SetCursorPos(native.x,native.y);
        INPUT mouse{}; mouse.type=INPUT_MOUSE; mouse.mi.dwFlags=MOUSEEVENTF_LEFTDOWN;
        QCOMPARE(SendInput(1,&mouse,sizeof(INPUT)),UINT(1)); QTest::qWait(40);
        watchdog.stop();
        mouse.mi.dwFlags=MOUSEEVENTF_LEFTUP;
        QCOMPARE(SendInput(1,&mouse,sizeof(INPUT)),UINT(1)); QTest::qWait(40);
        QVERIFY2(!openedOnPress,"Text dialog opened inside the mouse press handler");
        QPointer<QInputDialog> dialog;
        for(auto* w:QApplication::topLevelWidgets()) if(auto* d=qobject_cast<QInputDialog*>(w)) dialog=d;
        QVERIFY(dialog); QVERIFY(QTest::qWaitForWindowExposed(dialog));
        QCOMPARE(dialog->parentWidget(),toolbar);
        QTRY_COMPARE(GetForegroundWindow(),reinterpret_cast<HWND>(dialog->winId()));
        const auto text=QStringLiteral("文字标注");
        QList<INPUT> keys;
        for(auto character:text) {
            INPUT key{}; key.type=INPUT_KEYBOARD; key.ki.wScan=character.unicode(); key.ki.dwFlags=KEYEVENTF_UNICODE;
            keys.append(key); key.ki.dwFlags|=KEYEVENTF_KEYUP; keys.append(key);
        }
        QCOMPARE(SendInput(UINT(keys.size()),keys.data(),sizeof(INPUT)),UINT(keys.size()));
        QTRY_COMPARE(dialog->textValue(),text);
        if(finish=="desktop") session.setDrawing(false);
        else if(finish=="stop") session.stop();
        else if(finish=="escape") {
            INPUT key{}; key.type=INPUT_KEYBOARD; key.ki.wVk=VK_ESCAPE;
            QCOMPARE(SendInput(1,&key,sizeof(INPUT)),UINT(1));
            key.ki.dwFlags=KEYEVENTF_KEYUP; QCOMPARE(SendInput(1,&key,sizeof(INPUT)),UINT(1));
        }
        else {
            auto* buttons=dialog->findChild<QDialogButtonBox*>(); QVERIFY(buttons);
            auto* button=buttons->button(finish=="accept"?QDialogButtonBox::Ok:QDialogButtonBox::Cancel);
            QVERIFY(button); nativeClick(button,button->rect().center());
        }
        QTRY_VERIFY(dialog.isNull()); QVERIFY(!QApplication::activeModalWidget());
        if(finish=="stop") { QVERIFY(!session.active()); return; }
        QCOMPARE(session.document().history().count(),finish=="accept"?1:0);
        if(finish=="desktop") session.start(Theme::Dark);
        QAction* pen=nullptr;
        for(auto* a:toolbar->actions()) if(a->text()==QStringLiteral("画笔")) pen=a;
        QVERIFY(pen); QSignalSpy triggered(pen,&QAction::triggered);
        auto* button=toolbar->widgetForAction(pen); nativeClick(button,button->rect().center());
        QCOMPARE(triggered.size(),1);
        const auto before=session.document().history().count();
        nativeClick(overlay,QPoint(450,450));
        QCOMPARE(session.document().history().count(),before+1);
        session.stop();
    }
    void resumeExistingSession() {
        ScreenAnnotationSession session; session.start(Theme::Light);
        auto* document=&session.document();
        Annotation a; a.points={{400,400},{500,450}}; document->add(a);
        QToolBar* toolbar=nullptr;
        for(auto* w:QApplication::topLevelWidgets()) if(w->objectName()=="screen-annotation-toolbar") toolbar=qobject_cast<QToolBar*>(w);
        QVERIFY(toolbar);
        for(int i=0;i<3;++i) {
            session.setDrawing(false); toolbar->showMinimized();
            session.start(Theme::Light);
            QVERIFY(session.drawing()); QVERIFY(!toolbar->isMinimized());
            QCOMPARE(&session.document(),document); QCOMPARE(document->history().count(),1);
            QVERIFY(!toolbar->findChild<QAction*>("desktop-mode")->isChecked());
        }
        session.stop();
    }
    void toolSelectionAfterNativeDrawing() {
        POINT original{}; GetCursorPos(&original);
        const auto restore=qScopeGuard([&]{SetCursorPos(original.x,original.y);});
        ScreenAnnotationSession session; session.start(Theme::Dark);
        QWidget* overlay=nullptr; QToolBar* toolbar=nullptr;
        for(auto* w:QApplication::topLevelWidgets()) {
            if(w->objectName()=="screen-annotation-overlay" && w->screen()==QApplication::primaryScreen()) overlay=w;
            if(w->objectName()=="screen-annotation-toolbar") toolbar=qobject_cast<QToolBar*>(w);
        }
        QVERIFY(overlay); QVERIFY(toolbar); QVERIFY(QTest::qWaitForWindowExposed(toolbar));
        for(auto* action:toolbar->actions()) {
            if(!action->isCheckable() || action->objectName()=="desktop-mode") continue;
            if(action->text()==QStringLiteral("文字")) continue;
            nativeClick(overlay,QPoint(400,400));
            QVERIFY2(GetForegroundWindow()!=reinterpret_cast<HWND>(overlay->winId()),"Drawing layer stole foreground activation");
            auto* button=toolbar->widgetForAction(action); QVERIFY(button);
            const int before=session.document().history().count();
            QSignalSpy triggered(action,&QAction::triggered);
            nativeClick(button,button->rect().center());
            QCOMPARE(triggered.size(),1); QVERIFY(action->isChecked());
            QCOMPARE(session.document().history().count(),before);
        }
        for(auto* action:toolbar->actions()) if(action->text()==QStringLiteral("文字")) {
            auto* button=toolbar->widgetForAction(action); QVERIFY(button);
            nativeClick(button,button->rect().center()); QVERIFY(action->isChecked());
        }
        for(auto* combo:toolbar->findChildren<QComboBox*>()) {
            nativeClick(combo,combo->rect().center());
            QTRY_VERIFY(combo->view()->isVisible());
            const int index=(combo->currentIndex()+1)%combo->count();
            nativeClick(combo->view()->viewport(),combo->view()->visualRect(combo->model()->index(index,0)).center());
            QCOMPARE(combo->currentIndex(),index);
        }
        session.stop();
    }
    void liveDesktopAndPassthrough() {
        POINT original{}; GetCursorPos(&original);
        const auto restore=qScopeGuard([&]{SetCursorPos(original.x,original.y);});
        QPushButton background("desktop target"); background.resize(400,240);
        background.move(120,160); background.show(); background.raise();
        QVERIFY(QTest::qWaitForWindowExposed(&background));
        QSignalSpy clicked(&background,&QPushButton::clicked);
        ScreenAnnotationSession session; session.start(Theme::Dark);
        QVERIFY(session.active()); QVERIFY(session.drawing());
        QWidget* overlay=nullptr; QToolBar* toolbar=nullptr;
        for(auto* w:QApplication::topLevelWidgets()) {
            if(w->objectName()=="screen-annotation-overlay" && w->screen()==background.screen()) overlay=w;
            if(w->objectName()=="screen-annotation-toolbar") toolbar=qobject_cast<QToolBar*>(w);
        }
        QVERIFY(overlay); QVERIFY(toolbar); QVERIFY(QTest::qWaitForWindowExposed(overlay));
        const auto start=overlay->mapFromGlobal(background.mapToGlobal(QPoint(40,80)));
        QTest::mousePress(overlay,Qt::LeftButton,Qt::NoModifier,start);
        QTest::mouseMove(overlay,start+QPoint(160,0));
        QTest::mouseRelease(overlay,Qt::LeftButton,Qt::NoModifier,start+QPoint(160,0));
        QCOMPARE(session.document().history().count(),1);
        auto click=[&](QWidget* widget=nullptr) {
            if(!widget) widget=&background;
            auto* window=widget->window();
            const auto point=widget->mapTo(window,widget->rect().center())*window->devicePixelRatioF();
            POINT native{point.x(),point.y()}; ClientToScreen(reinterpret_cast<HWND>(window->winId()),&native);
            SetCursorPos(native.x,native.y);
            INPUT input[2]{}; input[0].type=input[1].type=INPUT_MOUSE;
            input[0].mi.dwFlags=MOUSEEVENTF_LEFTDOWN; input[1].mi.dwFlags=MOUSEEVENTF_LEFTUP;
            QCOMPARE(SendInput(2,input,sizeof(INPUT)),UINT(2)); QTest::qWait(120);
        };
        click(); QCOMPARE(clicked.count(),0);
        click(toolbar->widgetForAction(toolbar->findChild<QAction*>("desktop-mode"))); QVERIFY(!session.drawing());
        click(); QCOMPARE(clicked.count(),1);
        const auto imageBefore=background.screen()->grabWindow(0).toImage();
        background.setStyleSheet("background:#18cc42;"); QTest::qWait(200);
        QVERIFY(background.screen()->grabWindow(0).toImage()!=imageBefore);
        const auto history=session.document().history().count();
        click(toolbar->widgetForAction(toolbar->findChild<QAction*>("desktop-mode"))); QVERIFY(session.drawing());
        QCOMPARE(session.document().history().count(),history);
        click(); QCOMPARE(clicked.count(),1);
        QDir().mkpath("artifacts"); QVERIFY(toolbar->grab().save("artifacts/screen-annotation-toolbar.png"));
        toolbar->findChild<QAction*>("exit-annotation")->trigger();
        QVERIFY(!session.active()); QVERIFY(!session.drawing());
        QCoreApplication::sendPostedEvents(nullptr,QEvent::DeferredDelete);
        click(); QCOMPARE(clicked.count(),2);
        session.start(Theme::Light); QCOMPARE(session.document().history().count(),0);
        session.stop();
    }
};
QTEST_MAIN(ScreenAnnotationTests)
#include "screen_annotation_tests.moc"
