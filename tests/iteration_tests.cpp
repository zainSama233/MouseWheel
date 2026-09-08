#include <QtTest>
#include <QTemporaryDir>
#include <QComboBox>
#include <QPushButton>
#include <QLineEdit>
#include <QListWidget>
#include <QDir>
#include <QDoubleSpinBox>
#include <QWheelEvent>
#include "ui/style_editor.h"
#include "ui/localization.h"
#include "ui/library_icon.h"
#include "core/screen_helper.h"
#include <QPainter>
#include "config/config_store.h"
#include "ui/settings_window.h"
#include "ui/slot_editor.h"
#include "ui/wheel_window.h"
using namespace wheel;
class IterationTests:public QObject {
    Q_OBJECT
private Q_SLOTS:
    void cleanup(){Localization::instance().setLanguage(Language::SimplifiedChinese);}
    void languageSwitchPreservesDraft() {
        QTemporaryDir dir;ConfigStore store(dir.filePath("config.json"));QVERIFY(store.commit(defaultConfig()));
        SettingsWindow settings(store);settings.show();QVERIFY(QTest::qWaitForWindowExposed(&settings));
        auto* kind=settings.findChild<QComboBox*>("slot-kind-3");kind->setCurrentIndex(int(ActionKind::Application));
        auto* name=settings.findChild<QLineEdit*>("slot-name-3");name->setText(QStringLiteral("我的工作流"));
        auto* language=settings.findChild<QComboBox*>("language");language->setCurrentIndex(int(Language::English));
        QCOMPARE(store.current().language,Language::English);QCOMPARE(name->text(),QStringLiteral("我的工作流"));
        QCOMPARE(kind->currentText(),QString("Launch app"));QCOMPARE(settings.windowTitle(),QString("MouseWheel · Settings"));
        language->setCurrentIndex(int(Language::Japanese));QCOMPARE(name->text(),QStringLiteral("我的工作流"));QCOMPARE(kind->currentIndex(),int(ActionKind::Application));
        QCOMPARE(kind->currentText(),QStringLiteral("アプリを起動"));
        QTest::qWait(150);QDir().mkpath("artifacts");QVERIFY(settings.grab().save("artifacts/workspace-japanese.png"));
    }
    void sharedVectorRendersAtRequestedResolution() {
        const auto icon=libraryIcon("fixtures/library.svg");QVERIFY(!icon.isNull());
        const auto large=icon.pixmap(512,512);QCOMPARE(large.deviceIndependentSize(),QSizeF(512,512));QVERIFY(large.width()>=512);QVERIFY(!large.toImage().isNull());
        QVERIFY(libraryIcon("fixtures/missing.png").isNull());
    }
    void crossLevelSwapAndCenterRestrictions() {
        QTemporaryDir dir;ConfigStore store(dir.filePath("config.json"));auto c=defaultConfig();GroupAction group;group.slots[0]={"Child",ScreenshotAction{}};c.slots[3]={"Tools",group};QVERIFY(store.commit(c));
        SettingsWindow settings(store);auto* preview=settings.findChild<WheelWindow*>();
        Q_EMIT preview->positionsSwapped(-1,0,3,0);QCOMPARE(store.current().slots[0].name,QString("Child"));
        Q_EMIT preview->positionsSwapped(3,0,-1,-2);QCOMPARE(store.current().center,c.slots[0]);
        auto before=store.current();Q_EMIT preview->positionsSwapped(-1,3,-1,-2);QCOMPARE(store.current(),before);
        Q_EMIT preview->positionsSwapped(-1,3,3,1);QCOMPARE(store.current(),before);
    }
    void zoomPanKeepHitAligned() {
        WheelWindow preview(false);preview.resize(400,400);auto c=defaultConfig();preview.preview(c);preview.show();QVERIFY(QTest::qWaitForWindowExposed(&preview));
        const auto original=QPointF(200,200)+slotCenter(0)*400/(ScreenHelper::extent(c)*2);QSignalSpy selected(&preview,&WheelWindow::slotClicked);
        QWheelEvent wheel({200,200},preview.mapToGlobal(QPoint(200,200)),{},{0,120},Qt::NoButton,Qt::NoModifier,Qt::NoScrollPhase,false);QCoreApplication::sendEvent(&preview,&wheel);
        QTest::mousePress(&preview,Qt::MiddleButton,Qt::NoModifier,{200,200});QTest::mouseMove(&preview,{225,215});QTest::mouseRelease(&preview,Qt::MiddleButton,Qt::NoModifier,{225,215});
        QTest::mouseClick(&preview,Qt::LeftButton,Qt::NoModifier,(QPointF(225,215)+(original-QPointF(200,200))*1.15).toPoint());QCOMPARE(selected.last()[0].toInt(),0);
        preview.resetView();QTest::mouseClick(&preview,Qt::LeftButton,Qt::NoModifier,original.toPoint());QCOMPARE(selected.last()[0].toInt(),0);
        GroupAction group;group.slots[0]={"Child",ScreenshotAction{}};c.slots[0]={"Tools",group};preview.preview(c,0);QSignalSpy swaps(&preview,&WheelWindow::slotsSwapped);QSignalSpy back(&preview,&WheelWindow::levelRequested);
        QTest::mousePress(&preview,Qt::LeftButton,Qt::NoModifier,{200,200});QTest::mouseRelease(&preview,Qt::LeftButton,Qt::NoModifier,original.toPoint());QCOMPARE(swaps.count(),0);
        QTest::mouseClick(&preview,Qt::LeftButton,Qt::NoModifier,{200,200});QCOMPARE(back.count(),1);
    }
    void styleInheritanceAndRenderedExtent() {
        StyleEditor editor;SlotStyle base;base.text=QColor("#c0a090");base.iconSize=40;
        editor.setStyle(base);QCOMPARE(editor.style(),base);
        auto* size=editor.findChild<QDoubleSpinBox*>("style-number-1");size->setValue(80);
        QCOMPARE(editor.style().iconSize,std::optional<int>(80));
        auto c=defaultConfig();c.slots[0]={"Large icon",ScreenshotAction{}};c.slots[0].style=editor.style();c.slots[0].style.offsetY=-128;
        QVERIFY(ScreenHelper::extent(c)>250);
        const auto effective=cascadeStyle(base,c.slots[0].style);QCOMPARE(effective.text,base.text);
        editor.findChild<QLineEdit*>("style-color-3")->setText("#12");size->setValue(64);QCOMPARE(editor.style().text,base.text);
        editor.setStyle({});QCOMPARE(editor.style(),SlotStyle{});
    }
    void profileEditsAndCenterAreIndependent() {
        QTemporaryDir dir;ConfigStore store(dir.filePath("config.json"));auto c=defaultConfig();
        Profile profile;profile.id="editor";profile.name="Editor";c.profiles.append(profile);QVERIFY(store.commit(c));
        SettingsWindow settings(store);auto* selector=settings.findChild<QComboBox*>("profile-selector");QVERIFY(selector);
        selector->setCurrentIndex(selector->findData("editor"));
        settings.findChild<QComboBox*>("slot-kind-0")->setCurrentIndex(int(ActionKind::Screenshot));
        QCOMPARE(store.current().profiles[0].wheel.slots[0].kind(),ActionKind::Screenshot);
        QCOMPARE(store.current().slots[0],c.slots[0]);
        settings.findChild<QComboBox*>("slot-kind-12")->setCurrentIndex(int(ActionKind::Screenshot));
        QCOMPARE(store.current().profiles[0].wheel.center.kind(),ActionKind::Screenshot);
        selector->setCurrentIndex(0);QCOMPARE(settings.findChild<SlotEditor*>("slot-editor-12")->slot(),c.center);
    }
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
