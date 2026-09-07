#include <QtTest>
#include <QTemporaryDir>
#include <QComboBox>
#include <QLineEdit>
#include <QKeySequenceEdit>
#include <QLabel>
#include <QDir>
#include <QPointer>
#include "ui/settings_window.h"
#include "ui/wheel_window.h"
using namespace wheel;
class UiTests : public QObject {
    Q_OBJECT
private Q_SLOTS:
    void savesThemeAndAction() {
        QTemporaryDir dir;
        ConfigStore store(dir.filePath("config.json")); QVERIFY(store.load());
        SettingsWindow settings(store);
        settings.show();
        QVERIFY(QTest::qWaitForWindowExposed(&settings));
        auto combos = settings.findChildren<QComboBox*>();
        QCOMPARE(combos.size(),3);
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
        auto* recorder = settings.findChildren<QKeySequenceEdit*>().first();
        recorder->setFocus(); QTest::keyClick(recorder,Qt::Key_Tab);
        QTRY_COMPARE_WITH_TIMEOUT(store.current().slots[0].shortcut.key,int(Qt::Key_Tab),2000);
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
