#include <QtTest>
#include <QTemporaryDir>
#include <QComboBox>
#include "platform/application_catalog.h"
#include "platform/program_icon.h"
#include "platform/native_ui.h"
#include "ui/settings_window.h"
#include "config/config_store.h"
using namespace wheel;
class MacTests:public QObject {
    Q_OBJECT
private Q_SLOTS:
    void nativeOverlay() {
        QWidget overlay;Geometry geometry;geometry.center={200,200};geometry.radius=80;
        platform::placeOverlay(&overlay,geometry,true);
        QVERIFY(overlay.isVisible());QCOMPARE(overlay.size(),QSize(160,160));
        platform::setOverlayInput(&overlay,false);
        platform::setOverlayInput(&overlay,true);overlay.hide();
    }
    void appBundleProfile() {
        auto c=defaultConfig();Profile p;p.id="finder";p.name="Finder";p.applications={"/System/Library/CoreServices/Finder.app"};c.profiles.append(p);
        QVERIFY2(validate(c).isEmpty(),qPrintable(validate(c)));QCOMPARE(c.resolved(p.applications[0]).slots,p.wheel.slots);
        QVERIFY(!platform::programIcon(p.applications[0]).isNull());
    }
    void discoveryAndSettings() {
        const auto apps=platform::discoverApplications({"/System/Applications"});QVERIFY(!apps.isEmpty());
        QTemporaryDir dir;ConfigStore store(dir.filePath("config.json"));QVERIFY(store.commit(defaultConfig()));SettingsWindow settings(store);
        QVERIFY(settings.findChild<QComboBox*>("language"));
        QVERIFY(platform::defaultConfigPath().contains("Application Support"));
    }
};
QTEST_MAIN(MacTests)
#include "macos_tests.moc"
