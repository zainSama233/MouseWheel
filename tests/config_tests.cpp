#include <QtTest>
#include <QTemporaryDir>
#include <QFile>
#include <QBuffer>
#include <QImage>
#include "config/config_store.h"
using namespace wheel;
class ConfigTests : public QObject {
    Q_OBJECT
private Q_SLOTS:
    void launcherAppearanceRoundTrip() {
        QTemporaryDir dir; ConfigStore store(dir.filePath("config.json"));
        auto config=defaultConfig(); config.shape=WheelShape::Hexagon;
        QImage image(96,96,QImage::Format_ARGB32); image.fill(Qt::red);
        QBuffer buffer(&config.centerImage); QVERIFY(buffer.open(QIODevice::WriteOnly)); QVERIFY(image.save(&buffer,"PNG"));
        config.slots[0]={QStringLiteral("应用"),{},ActionKind::Application,"C:/应用 空格/app.exe"};
        config.slots[1]={QStringLiteral("网站"),{},ActionKind::Website,"https://example.com/path?q=a&b=2"};
        QVERIFY(store.commit(config)); ConfigStore reopened(store.path()); QVERIFY(reopened.load()); QCOMPARE(reopened.current(),config);
        config.centerImage="invalid"; QVERIFY(!store.commit(config));
    }
    void screenshotRoundTripAndValidation() {
        QTemporaryDir dir; ConfigStore store(dir.filePath("config.json"));
        auto config=defaultConfig(); config.slots[0]={QStringLiteral("截图"),{},ActionKind::Screenshot};
        QVERIFY(store.commit(config)); ConfigStore reopened(store.path()); QVERIFY(reopened.load());
        QCOMPARE(reopened.current(),config);
        config.slots[0].shortcut.key=Qt::Key_C; QVERIFY(!store.commit(config));
    }
    void singleMiddleRoundTrip() {
        QTemporaryDir dir;
        ConfigStore store(dir.filePath("config.json"));
        auto c = defaultConfig(); c.modifier = static_cast<Modifier>(0); c.button = MouseButton::Middle;
        QVERIFY(store.commit(c));
        ConfigStore reopened(store.path()); QVERIFY(reopened.load());
        QCOMPARE(reopened.current(), c);
        c.button = MouseButton::Right; QVERIFY(!store.commit(c));
        QCOMPARE(store.current(), reopened.current());
    }
    void roundTrip() {
        QTemporaryDir dir;
        ConfigStore store(dir.filePath("config.json"));
        QVERIFY(store.load());
        auto config = store.current();
        config.theme = Theme::Warm;
        config.slots[0].name = QString::fromUtf8("复制");
        QVERIFY(store.commit(config));
        ConfigStore reopened(store.path());
        QVERIFY(reopened.load());
        QCOMPARE(reopened.current(), config);
    }
    void corruptFilePreserved() {
        QTemporaryDir dir;
        const auto path = dir.filePath("config.json");
        QFile f(path); QVERIFY(f.open(QIODevice::WriteOnly)); f.write("{broken"); f.close();
        ConfigStore store(path);
        QVERIFY(!store.load());
        QVERIFY(!store.commit(defaultConfig()));
        QVERIFY(f.open(QIODevice::ReadOnly)); QCOMPARE(f.readAll(), QByteArray("{broken")); f.close();
        QVERIFY(store.reset());
        QVERIFY(store.load());
    }
    void failedSaveDoesNotPublish() {
        QTemporaryDir dir;
        ConfigStore store(dir.filePath("missing/config.json"));
        QVERIFY(store.load());
        auto c = store.current(); c.theme = Theme::Dark;
        QSignalSpy spy(&store, &ConfigStore::changed);
        QVERIFY(!store.commit(c));
        QCOMPARE(store.current(), defaultConfig());
        QCOMPARE(spy.count(), 0);
    }
    void invalidDraft() {
        QTemporaryDir dir;
        ConfigStore store(dir.filePath("config.json"));
        QVERIFY(store.load());
        auto c = store.current(); c.slots[0].shortcut.key = Qt::Key_Control;
        QVERIFY(!store.commit(c));
        QVERIFY(!QFile::exists(store.path()));
    }
    void unsupportedVersionPreserved() {
        QTemporaryDir dir;
        QFile file(dir.filePath("config.json"));
        QVERIFY(file.open(QIODevice::WriteOnly));
        file.write(R"({"version":999})"); file.close();
        ConfigStore store(file.fileName());
        QVERIFY(!store.load());
        QVERIFY(store.blocked());
    }
};
QTEST_GUILESS_MAIN(ConfigTests)
#include "config_tests.moc"
