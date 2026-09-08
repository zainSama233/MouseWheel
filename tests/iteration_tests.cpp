#include <QtTest>
#include <QTemporaryDir>
#include <QComboBox>
#include <QPushButton>
#include <QLineEdit>
#include <QListWidget>
#include <QDir>
#include <QPainter>
#include "config/config_store.h"
#include "ui/settings_window.h"
#include "ui/slot_editor.h"
#include "ui/wheel_window.h"
using namespace wheel;
class IterationTests:public QObject {
    Q_OBJECT
private Q_SLOTS:
    void failedResizeKeepsSavedCount() {
        QTemporaryDir dir;ConfigStore store(dir.filePath("missing/config.json"));QVERIFY(store.load());SettingsWindow settings(store);
        auto* count=settings.findChild<QComboBox*>("wheel-count");count->setCurrentIndex(count->findData(12));
        QCOMPARE(store.current().slots.size(),8);QCOMPARE(count->currentData().toInt(),8);
    }
    void groupedEditingAndShrinkProtection() {
        QTemporaryDir dir;ConfigStore store(dir.filePath("config.json"));QVERIFY(store.commit(defaultConfig()));
        SettingsWindow settings(store);settings.show();QVERIFY(QTest::qWaitForWindowExposed(&settings));
        auto* count=settings.findChild<QComboBox*>("wheel-count");count->setCurrentIndex(count->findData(12));
        QCOMPARE(store.current().slots.size(),12);
        settings.findChild<QListWidget*>("slot-list")->setCurrentRow(11);
        settings.findChild<QComboBox*>("slot-kind-11")->setCurrentIndex(int(ActionKind::Group));
        QCOMPARE(store.current().slots[11].kind(),ActionKind::Group);
        QTest::mouseClick(settings.findChild<QPushButton*>("slot-edit-group-11"),Qt::LeftButton);
        auto* nav=settings.findChild<QComboBox*>("wheel-navigation");QCOMPARE(nav->currentData().toInt(),11);
        count->setCurrentIndex(count->findData(4));
        settings.findChild<QComboBox*>("slot-kind-0")->setCurrentIndex(int(ActionKind::Screenshot));
        settings.findChild<QLineEdit*>("slot-name-0")->setText("Child pin");
        QCOMPARE(std::get<GroupAction>(store.current().slots[11].action).slots.size(),4);
        QCOMPARE(std::get<GroupAction>(store.current().slots[11].action).slots[0].name,QString("Child pin"));
        QCOMPARE(settings.findChild<QComboBox*>("slot-kind-0")->findText(actionKindName(ActionKind::Group)),-1);
        nav->setCurrentIndex(nav->findData(-1));QCOMPARE(nav->currentData().toInt(),-1);
        count->setCurrentIndex(count->findData(4));QCOMPARE(store.current().slots.size(),12);QCOMPARE(count->currentData().toInt(),12);
        auto* preview=settings.findChild<WheelWindow*>();Q_EMIT preview->slotsSwapped(11,2);
        QCOMPARE(store.current().slots[2].kind(),ActionKind::Group);QVERIFY(!store.current().slots[11].enabled());
        count->setCurrentIndex(count->findData(4));QCOMPARE(store.current().slots.size(),4);
        ConfigStore reopened(store.path());QVERIFY(reopened.load());QCOMPARE(reopened.current(),store.current());
        QDir().mkpath("artifacts");QVERIFY(settings.grab().save("artifacts/iteration-settings.png"));
    }
    void incompleteDraftSurvivesNavigationAttempt() {
        QTemporaryDir dir;ConfigStore store(dir.filePath("config.json"));auto config=defaultConfig();config.slots[2]={"Tools",GroupAction{}};QVERIFY(store.commit(config));
        SettingsWindow settings(store);
        settings.findChild<QComboBox*>("slot-kind-3")->setCurrentIndex(int(ActionKind::Application));
        auto* nav=settings.findChild<QComboBox*>("wheel-navigation");nav->setCurrentIndex(nav->findData(2));
        QCOMPARE(nav->currentData().toInt(),-1);QCOMPARE(settings.findChild<SlotEditor*>("slot-editor-3")->slot().kind(),ActionKind::Application);
        settings.findChild<QComboBox*>("theme")->setCurrentIndex(int(Theme::Dark));QCOMPARE(store.current().theme,Theme::Dark);
    }
    void layoutGallery() {
        QImage gallery(4*328,3*360,QImage::Format_ARGB32);gallery.fill(QColor("#19202e"));QPainter painter(&gallery);
        QDir().mkpath("artifacts");int row=0;
        for(int count:{4,8,12}) {
            int col=0;
            for(auto shape:{WheelShape::Original,WheelShape::Circle,WheelShape::Capsule,WheelShape::HexagonHive}) {
                auto config=defaultConfig();config.slots.resize(count);config.shape=shape;config.theme=Theme::Dark;
                for(int i=0;i<count;++i) config.slots[i]={QString::number(i+1),SystemAction{SystemOperation(i%13)}};
                WheelWindow preview(false);preview.preview(config);preview.resize(328,328);preview.show();QVERIFY(QTest::qWaitForWindowExposed(&preview));
                painter.drawPixmap(col*328,row*360,preview.grab());painter.setPen(Qt::white);
                painter.drawText(QRect(col*328,row*360+330,328,25),Qt::AlignCenter,QStringList{"Original","Circle","Capsule","HexagonHive"}[col]+QString(" / %1").arg(count));++col;
            }
            ++row;
        }
        painter.end();QVERIFY(gallery.save("artifacts/iteration-shapes.png"));
    }
};
QTEST_MAIN(IterationTests)
#include "iteration_tests.moc"
