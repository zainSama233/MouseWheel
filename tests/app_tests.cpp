#include <QtTest>
#include <QMenu>
#include <QAction>
#include <QPointer>
#include <QScopeGuard>
#include <Windows.h>
#include "app.h"
#include "ui/settings_window.h"
#include "tools/pinned_image.h"
using namespace wheel;
class AppTests final : public QObject {
    Q_OBJECT
private Q_SLOTS:
    void independentToolsAndSettings() {
        QTemporaryDir dir; ConfigStore store(dir.filePath("config.json")); QVERIFY(store.load()); QVERIFY(store.commit(defaultConfig()));
        App app(store.path()); app.start(false);
        QMenu* menu=nullptr;
        for(auto* w:QApplication::topLevelWidgets()) if(auto* candidate=qobject_cast<QMenu*>(w))
            for(auto* action:candidate->actions()) if(action->text()==QStringLiteral("打开设置")) menu=candidate;
        QVERIFY(menu);
        auto invoke=[menu](const QString& label) {
            for(auto* action:menu->actions()) if(action->text()==label) { action->trigger(); return true; }
            return false;
        };
        QVERIFY(invoke(actionKindName(ActionKind::Screenshot)));
        QWidget* picker=nullptr;
        for(auto* w:QApplication::topLevelWidgets()) if(w->objectName()=="region-picker") picker=w;
        QVERIFY(picker); QVERIFY(QTest::qWaitForWindowExposed(picker));
        QTest::mousePress(picker,Qt::LeftButton,Qt::NoModifier,QPoint(100,100));
        QTest::mouseRelease(picker,Qt::LeftButton,Qt::NoModifier,QPoint(300,250));
        QPointer<PinnedImage> pin;
        QTRY_VERIFY(([&]{for(auto* w:QApplication::topLevelWidgets()) if(auto* p=qobject_cast<PinnedImage*>(w)) pin=p; return bool(pin);})());
        QVERIFY(invoke(QStringLiteral("打开设置")));
        QPointer<SettingsWindow> settings;
        for(auto* w:QApplication::topLevelWidgets()) if(auto* s=qobject_cast<SettingsWindow*>(w)) settings=s;
        QVERIFY(settings); QVERIFY(settings->isVisible()); QVERIFY(pin->isVisible());
        settings->close(); QTRY_VERIFY(!settings);
        POINT cursor{}; GetCursorPos(&cursor);
        const auto restore=qScopeGuard([&]{SetCursorPos(cursor.x,cursor.y);});
        WheelWindow* wheel=nullptr;
        for(auto* w:QApplication::topLevelWidgets()) if(auto* found=qobject_cast<WheelWindow*>(w)) wheel=found;
        QVERIFY(wheel); QTest::qWait(100);
        INPUT middle{}; middle.type=INPUT_MOUSE; middle.mi.dwFlags=MOUSEEVENTF_MIDDLEDOWN; middle.mi.dwExtraInfo=0x54455354;
        bool held=true;
        const auto release=qScopeGuard([&]{if(held) { middle.mi.dwFlags=MOUSEEVENTF_MIDDLEUP; SendInput(1,&middle,sizeof(INPUT)); }});
        QCOMPARE(SendInput(1,&middle,sizeof(INPUT)),UINT(1));
        QTRY_VERIFY(wheel->isVisible());
        RECT bounds{}; GetWindowRect(reinterpret_cast<HWND>(wheel->winId()),&bounds);
        SetCursorPos((bounds.left+bounds.right)/2,(bounds.top+bounds.bottom)/2);
        middle.mi.dwFlags=MOUSEEVENTF_MIDDLEUP; QCOMPARE(SendInput(1,&middle,sizeof(INPUT)),UINT(1)); held=false;
        QTRY_VERIFY(!wheel->isVisible()); QVERIFY(pin->isVisible());
        QVERIFY(invoke(actionKindName(ActionKind::ScreenAnnotation)));
        QWidget* toolbar=nullptr;
        for(auto* w:QApplication::topLevelWidgets()) if(w->objectName()=="screen-annotation-toolbar") toolbar=w;
        QVERIFY(toolbar); QVERIFY(toolbar->isVisible()); QVERIFY(pin->isVisible());
        QVERIFY(invoke(QStringLiteral("打开设置")));
        for(auto* w:QApplication::topLevelWidgets()) if(auto* s=qobject_cast<SettingsWindow*>(w)) settings=s;
        QVERIFY(settings); QVERIFY(settings->isVisible());
        QVERIFY(toolbar->findChild<QAction*>("desktop-mode")->isChecked());
        toolbar->findChild<QAction*>("exit-annotation")->trigger();
        settings->close(); QTRY_VERIFY(!settings);
        QVERIFY(invoke(actionKindName(ActionKind::Screenshot)));
        QVERIFY(invoke(QStringLiteral("打开设置")));
        for(auto* w:QApplication::topLevelWidgets()) {
            QVERIFY(w->objectName()!="region-picker");
            if(auto* s=qobject_cast<SettingsWindow*>(w)) settings=s;
        }
        QVERIFY(settings && settings->isVisible()); QVERIFY(pin->isVisible());
        settings->close(); pin->close(); QTRY_VERIFY(!settings && !pin);
    }
};
QTEST_MAIN(AppTests)
#include "app_tests.moc"
