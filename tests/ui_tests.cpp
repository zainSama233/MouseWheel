#include <QtTest>
#include <QTemporaryDir>
#include <QComboBox>
#include <QPushButton>
#include <Windows.h>
#include "ui/slot_editor.h"
#include "ui/shortcut_editor.h"
#include "platform/windows_injection.h"
#include "platform/shortcut_capture.h"
#include <QLineEdit>
#include <QKeySequenceEdit>
#include <QLabel>
#include <QDir>
#include <QPointer>
#include <QVariantAnimation>
#include <QScreen>
#include <QBuffer>
#include <QPainter>
#include "core/image_asset.h"
#include "ui/action_icons.h"
#include "ui/settings_window.h"
#include "ui/wheel_window.h"
using namespace wheel;
class UiTests : public QObject {
    Q_OBJECT
private Q_SLOTS:
    void iconSurvivesActionChange() {
        QTemporaryDir dir; ConfigStore store(dir.filePath("config.json")); QVERIFY(store.load()); SettingsWindow settings(store);
        auto* symbol=settings.findChild<QComboBox*>("slot-symbol-2"); symbol->setCurrentIndex(symbol->findData("copy"));
        auto* kind=settings.findChild<QComboBox*>("slot-kind-2"); kind->setCurrentIndex(int(ActionKind::Website));
        auto* url=settings.findChild<QLineEdit*>("slot-target-2"); url->setText("https://example.com");QMetaObject::invokeMethod(url,"editingFinished");
        QCOMPARE(store.current().slots[2].icon.value,QString("copy"));
        kind->setCurrentIndex(int(ActionKind::Screenshot)); QCOMPARE(store.current().slots[2].icon.value,QString("copy"));
        QVERIFY(store.current().slots[2].kind()==ActionKind::Screenshot);
    }
    void exclusiveCaptureAndPauseBreak() {
        ShortcutEditor editor; editor.show(); editor.activateWindow();
        QVERIFY(QTest::qWaitForWindowExposed(&editor)); SetForegroundWindow(reinterpret_cast<HWND>(editor.winId())); QTest::qWait(100);
        QSignalSpy edited(&editor,&ShortcutEditor::edited);
        auto* button=editor.findChild<QPushButton*>(); QVERIFY(button);QTest::mouseClick(button,Qt::LeftButton); edited.clear();
        auto down=win::keyEvent(VK_LCONTROL,true),key=win::keyEvent('P',true),up=win::keyEvent('P',false),release=win::keyEvent(VK_LCONTROL,false);
        INPUT events[]{down,key,up,release};for(auto& event:events)event.ki.dwExtraInfo=0;
        QCOMPARE(SendInput(4,events,sizeof(INPUT)),4u); QTRY_COMPARE(edited.size(),1);
        QCOMPARE(editor.shortcut(),(Shortcut{Qt::Key_P,bit(Modifier::Control)}));
        auto* combo=editor.findChild<QComboBox*>(); QVERIFY(combo->findData(int(Qt::Key_Pause))>=0);QVERIFY(combo->findData(int(Qt::Key_Cancel))>=0);
        QTest::mouseClick(button,Qt::LeftButton);QTest::mouseClick(button,Qt::LeftButton);QTest::qWait(50);QCOMPARE(edited.size(),1);
        editor.setShortcut({Qt::Key_Cancel,0});QCOMPARE(editor.shortcut().key,int(Qt::Key_Cancel));
        editor.setShortcut({Qt::Key_Pause,0});QCOMPARE(editor.shortcut().key,int(Qt::Key_Pause));
    }
    void closingRecorderDrainsHeldKeys() {
        auto* editor=new ShortcutEditor;editor->show();editor->activateWindow();QVERIFY(QTest::qWaitForWindowExposed(editor));SetForegroundWindow(reinterpret_cast<HWND>(editor->winId()));QTest::qWait(100);
        QTest::mouseClick(editor->findChild<QPushButton*>(),Qt::LeftButton);QVERIFY(ShortcutCapture::active());
        auto down=win::keyEvent(VK_LCONTROL,true);down.ki.dwExtraInfo=0;QCOMPARE(SendInput(1,&down,sizeof(INPUT)),1u);QTest::qWait(50);
        delete editor;auto up=win::keyEvent(VK_LCONTROL,false);up.ki.dwExtraInfo=0;QCOMPARE(SendInput(1,&up,sizeof(INPUT)),1u);
        QTRY_VERIFY(!ShortcutCapture::active());
    }
    void hidingRecorderCancelsCapture() {
        ShortcutEditor editor;editor.show();editor.activateWindow();QVERIFY(QTest::qWaitForWindowExposed(&editor));SetForegroundWindow(reinterpret_cast<HWND>(editor.winId()));QTest::qWait(100);
        QTest::mouseClick(editor.findChild<QPushButton*>(),Qt::LeftButton);QVERIFY(ShortcutCapture::active());editor.hide();QTRY_VERIFY(!ShortcutCapture::active());
    }
    void appearanceAndTargetPersist() {
        QTemporaryDir dir; ConfigStore store(dir.filePath("config.json")); QVERIFY(store.load());
        SettingsWindow settings(store);
        auto* shape=settings.findChild<QComboBox*>("wheel-shape"); QVERIFY(shape);
        shape->setCurrentIndex(2); QCOMPARE(store.current().shape,WheelShape::Hexagon);
        auto* kind=settings.findChild<QComboBox*>("slot-kind-2"); kind->setCurrentIndex(int(ActionKind::Website));
        auto* target=settings.findChild<QLineEdit*>("slot-target-2"); QVERIFY(target);
        target->setText("example.com/path?q=1"); QMetaObject::invokeMethod(target,"editingFinished");
        QCOMPARE(std::get<WebsiteAction>(store.current().slots[2].action).url,QString("https://example.com/path?q=1"));
        ConfigStore reopened(store.path()); QVERIFY(reopened.load()); QCOMPARE(reopened.current(),store.current());
    }
    void importedImageAndShapePreviews() {
        QTemporaryDir dir;
        QImage source(180,120,QImage::Format_ARGB32); source.fill(QColor("#c44569"));
        { QPainter p(&source); p.setPen(QPen(Qt::white,12)); p.drawEllipse(30,15,120,90); }
        const auto path=dir.filePath("center.png"); QVERIFY(source.save(path));
        auto config=defaultConfig(); QString error; QVERIFY(importImageAsset(path,config.centerImage,error));
        QFile::remove(path); QVERIFY(!decodeImageAsset(config.centerImage).isNull());
        config.slots[2]={"复制",Shortcut{Qt::Key_C,bit(Modifier::Control)}};
        config.slots[3]={"粘贴",Shortcut{Qt::Key_V,bit(Modifier::Control)}};
        config.slots[4]={"网页",WebsiteAction{"https://example.com"}};
        QVERIFY(!actionIcon(config.slots[2],Qt::black).isNull());
        QVERIFY(actionIcon(config.slots[2],Qt::black).pixmap(32,32).toImage()!=actionIcon(config.slots[3],Qt::black).pixmap(32,32).toImage());
        QDir().mkpath("artifacts");
        for(int shape=0;shape<3;++shape) for(int theme=0;theme<3;++theme) {
            config.shape=static_cast<WheelShape>(shape); config.theme=static_cast<Theme>(theme);
            WheelWindow preview(false); preview.preview(config); preview.show();
            QVERIFY(QTest::qWaitForWindowExposed(&preview));
            QVERIFY(preview.grab().save(QString("artifacts/wheel-%1-%2.png").arg(shape).arg(theme)));
        }
    }
    void animationStopsOnQuickDismiss() {
        WheelWindow wheel; const auto geometry=Geometry::fit({500,500},{0,0,1920,1080},1);
        wheel.present(1,defaultConfig(),geometry,QApplication::primaryScreen()->name());
        auto* animation=wheel.findChild<QVariantAnimation*>(); QVERIFY(animation);
        QCOMPARE(animation->state(),QAbstractAnimation::Running);
        QSignalSpy hidden(&wheel,&WheelWindow::hidden); wheel.dismiss(1);
        QVERIFY(!wheel.isVisible()); QCOMPARE(animation->state(),QAbstractAnimation::Stopped); QCOMPARE(hidden.size(),1);
        QTest::qWait(180); QVERIFY(!wheel.isVisible());
        wheel.present(2,defaultConfig(),geometry,QApplication::primaryScreen()->name());
        QTRY_COMPARE(animation->state(),QAbstractAnimation::Stopped); QVERIFY(wheel.isVisible()); wheel.dismiss(2);
    }
    void toolSlotPersists_data() {
        QTest::addColumn<int>("actionKind");
        QTest::newRow("pin")<<1; QTest::newRow("screen annotation")<<2;
    }
    void toolSlotPersists() {
        QFETCH(int,actionKind);
        QTemporaryDir dir; ConfigStore store(dir.filePath("config.json")); QVERIFY(store.load());
        SettingsWindow settings(store);
        auto* kind=settings.findChild<QComboBox*>("slot-kind-0"); QVERIFY(kind);
        kind->setCurrentIndex(actionKind);
        QCOMPARE(store.current().slots[0].kind(),static_cast<ActionKind>(actionKind));
        QVERIFY(!std::holds_alternative<Shortcut>(store.current().slots[0].action));
        ConfigStore reopened(store.path()); QVERIFY(reopened.load());
        QCOMPARE(reopened.current(),store.current());
    }
    void selectsSingleMiddleAndPersists() {
        QTemporaryDir dir; ConfigStore store(dir.filePath("config.json"));
        auto c = defaultConfig(); c.modifier = Modifier::Control; c.button = MouseButton::Right;
        QVERIFY(store.commit(c));
        SettingsWindow settings(store);
        auto* modifier = settings.findChild<QComboBox*>("trigger-modifier");
        auto* button = settings.findChild<QComboBox*>("trigger-button");
        QVERIFY(modifier); QVERIFY(button);
        modifier->setCurrentIndex(modifier->findData(static_cast<int>(Modifier::None)));
        QCOMPARE(store.current().modifier, Modifier::None);
        QCOMPARE(store.current().button, MouseButton::Middle);
        QVERIFY(!button->isEnabled());
        ConfigStore reopened(store.path()); QVERIFY(reopened.load());
        QCOMPARE(reopened.current(), store.current());
        modifier->setCurrentIndex(modifier->findData(static_cast<int>(Modifier::Control)));
        QVERIFY(button->isEnabled()); button->setCurrentIndex(static_cast<int>(MouseButton::Right));
        QCOMPARE(store.current().button, MouseButton::Right);
    }
    void savesThemeAndAction() {
        QTemporaryDir dir;
        ConfigStore store(dir.filePath("config.json")); QVERIFY(store.load());
        SettingsWindow settings(store);
        settings.show();
        QVERIFY(QTest::qWaitForWindowExposed(&settings));
        auto combos = settings.findChildren<QComboBox*>();
        QVERIFY(combos.size()>12);
        combos[2]->setCurrentIndex(2);
        QCOMPARE(store.current().theme,Theme::Dark);
        QList<QLineEdit*> names;
        for (int i=0;i<8;++i) names.append(settings.findChild<QLineEdit*>(QString("slot-name-%1").arg(i)));
        names[0]->setText(QStringLiteral("测试"));
        QMetaObject::invokeMethod(names[0],"editingFinished");
        QCOMPARE(store.current().slots[0].name,QStringLiteral("测试"));
        ConfigStore reopened(store.path()); QVERIFY(reopened.load());
        QCOMPARE(reopened.current(),store.current());
    }
    void closingReleasesSettings() {
        QTemporaryDir dir;
        ConfigStore store(dir.filePath("config.json")); QVERIFY(store.load());
        for (int i=0;i<3;++i) {
            QPointer<SettingsWindow> settings = new SettingsWindow(store);
            settings->show(); QVERIFY(QTest::qWaitForWindowExposed(settings));
            settings->close(); QTRY_VERIFY(settings.isNull());
        }
    }

    void recordsTabShortcut() {
        QTemporaryDir dir;
        ConfigStore store(dir.filePath("config.json")); QVERIFY(store.load());
        SettingsWindow settings(store); settings.show();
        settings.findChild<QComboBox*>("slot-kind-0")->setCurrentIndex(0);
        auto* recorder = settings.findChild<SlotEditor*>("slot-editor-0")->findChild<QKeySequenceEdit*>();
        recorder->setFocus(); QTest::keyClick(recorder,Qt::Key_Tab);
        QTRY_VERIFY_WITH_TIMEOUT(std::holds_alternative<Shortcut>(store.current().slots[0].action),2000);
        QTRY_COMPARE_WITH_TIMEOUT(std::get<Shortcut>(store.current().slots[0].action).key,int(Qt::Key_Tab),2000);
    }

    void renderThemes() {
        QTemporaryDir dir;
        ConfigStore store(dir.filePath("config.json")); QVERIFY(store.load());
        QDir().mkpath("artifacts");
        for (int theme=0;theme<3;++theme) {
            auto config = defaultConfig(); config.theme = static_cast<Theme>(theme);
            QVERIFY(store.commit(config));
            SettingsWindow settings(store); settings.show();
            QVERIFY(QTest::qWaitForWindowExposed(&settings));
            QVERIFY(settings.grab().save(QString("artifacts/settings-%1.png").arg(theme)));
        }
    }
    void staleNotificationsCannotReopenWheel() {
        WheelWindow wheel;
        wheel.dismiss(4);
        wheel.present(3,defaultConfig(),Geometry{},"");
        QVERIFY(!wheel.isVisible());
    }
};
QTEST_MAIN(UiTests)
#include "ui_tests.moc"
