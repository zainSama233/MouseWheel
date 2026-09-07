#include <QtTest>
#include <QTemporaryDir>
#include <QFile>
#include "config/config_store.h"
using namespace wheel;
class ConfigTests : public QObject {
    Q_OBJECT
private Q_SLOTS:
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
