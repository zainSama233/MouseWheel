#include <QtTest>
#include <QPushButton>
#include <QToolBar>
#include <QAction>
#include <QComboBox>
#include <QAbstractItemView>
#include <QScreen>
#include <QScopeGuard>
#include <Windows.h>
#include "tools/screen_annotation_session.h"
#include "tools/screenshot_session.h"
using namespace wheel;
class ScreenAnnotationTests final : public QObject {
    Q_OBJECT
private Q_SLOTS:
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
        auto click=[](QWidget* widget,QPoint point) {
            auto* window=widget->window(); const auto local=widget->mapTo(window,point)*window->devicePixelRatioF();
            POINT native{local.x(),local.y()}; ClientToScreen(reinterpret_cast<HWND>(window->winId()),&native);
            SetCursorPos(native.x,native.y);
            INPUT event{}; event.type=INPUT_MOUSE; event.mi.dwFlags=MOUSEEVENTF_LEFTDOWN;
            QCOMPARE(SendInput(1,&event,sizeof(INPUT)),UINT(1)); QTest::qWait(100);
            event.mi.dwFlags=MOUSEEVENTF_LEFTUP;
            QCOMPARE(SendInput(1,&event,sizeof(INPUT)),UINT(1)); QTest::qWait(100);
        };
        for(auto* action:toolbar->actions()) {
            if(!action->isCheckable() || action->objectName()=="desktop-mode") continue;
            if(action->text()==QStringLiteral("文字")) continue;
            click(overlay,QPoint(400,400));
            QVERIFY2(GetForegroundWindow()!=reinterpret_cast<HWND>(overlay->winId()),"Drawing layer stole foreground activation");
            auto* button=toolbar->widgetForAction(action); QVERIFY(button);
            const int before=session.document().history().count();
            QSignalSpy triggered(action,&QAction::triggered);
            click(button,button->rect().center());
            QCOMPARE(triggered.size(),1); QVERIFY(action->isChecked());
            QCOMPARE(session.document().history().count(),before);
        }
        for(auto* action:toolbar->actions()) if(action->text()==QStringLiteral("文字")) {
            auto* button=toolbar->widgetForAction(action); QVERIFY(button);
            click(button,button->rect().center()); QVERIFY(action->isChecked());
        }
        for(auto* combo:toolbar->findChildren<QComboBox*>()) {
            click(combo,combo->rect().center());
            QTRY_VERIFY(combo->view()->isVisible());
            const int index=(combo->currentIndex()+1)%combo->count();
            click(combo->view()->viewport(),combo->view()->visualRect(combo->model()->index(index,0)).center());
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
