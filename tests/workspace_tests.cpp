#include <QtTest>
#include <QTemporaryDir>
#include <QFile>
#include <QBuffer>
#include <QImage>
#include <QDir>
#include <QDataStream>
#include "config/config_store.h"
using namespace wheel;
class WorkspaceTests:public QObject {
    Q_OBJECT
private Q_SLOTS:
    void rasterImportsAndFailedWrite() {
        QTemporaryDir dir;ConfigStore store(dir.filePath("config.json"));QVERIFY(store.commit(defaultConfig()));
        QImage image(32,32,QImage::Format_ARGB32);image.fill(Qt::green);
        for(const QString format:{"png","jpg"}){const auto path=dir.filePath("source."+format);QVERIFY(image.save(path,format.toLatin1()));const auto id=store.importIcon(path);QVERIFY2(!id.isEmpty(),qPrintable(store.error()));QVERIFY(QFile::remove(path));QVERIFY(QFile::exists(store.assetPath(id)));}
        QByteArray png;QBuffer pngBuffer(&png);pngBuffer.open(QIODevice::WriteOnly);QVERIFY(image.save(&pngBuffer,"PNG"));
        QByteArray ico;QDataStream stream(&ico,QIODevice::WriteOnly);stream.setByteOrder(QDataStream::LittleEndian);stream<<quint16(0)<<quint16(1)<<quint16(1)<<quint8(32)<<quint8(32)<<quint8(0)<<quint8(0)<<quint16(1)<<quint16(32)<<quint32(png.size())<<quint32(22);ico.append(png);
        QFile file(dir.filePath("source.ico"));QVERIFY(file.open(QIODevice::WriteOnly));file.write(ico);file.close();QVERIFY(!store.importIcon(file.fileName()).isEmpty());
        const auto before=store.current();QVERIFY(QFile::remove(store.path()));QVERIFY(QDir().mkdir(store.path()));
        const int files=QDir(store.assetDirectory()).entryList(QDir::Files).size();QVERIFY(store.importIcon("fixtures/library.svg").isEmpty());QCOMPARE(store.current(),before);QCOMPARE(QDir(store.assetDirectory()).entryList(QDir::Files).size(),files);
    }
    void migrateEmbeddedIconsTogether() {
        QTemporaryDir dir;ConfigStore store(dir.filePath("config.json"));auto c=defaultConfig();
        QImage image(16,16,QImage::Format_ARGB32);image.fill(Qt::red);QByteArray png;QBuffer buffer(&png);buffer.open(QIODevice::WriteOnly);QVERIFY(image.save(&buffer,"PNG"));
        c.slots[0].icon={IconSource::Image,{},png};c.centerImage=png;QVERIFY(store.commit(c));
        QVERIFY(store.migrateIcons());QCOMPARE(store.current().assets.size(),1);QVERIFY(store.current().centerImage.isEmpty());
        QCOMPARE(store.current().slots[0].icon.source,IconSource::Library);QCOMPARE(store.current().center.icon.value,store.current().slots[0].icon.value);
        ConfigStore reopened(store.path());QVERIFY(reopened.load());QCOMPARE(reopened.current(),store.current());QVERIFY(store.migrateIcons());QCOMPARE(store.current().assets.size(),1);
    }
    void profilesAndStylesRoundTrip() {
        QTemporaryDir dir;ConfigStore store(dir.filePath("config.json"));Config c=defaultConfig();
        Profile profile;profile.id="work";profile.name="Work";profile.applications={"C:/Apps/Code.exe"};
        profile.wheel.centerEnabled=true;profile.wheel.center={"Center",ScreenshotAction{}};
        profile.wheel.style.text=QColor("#ccaa99");profile.wheel.slots[0].style.iconSize=42;
        c.profiles.append(profile);c.language=Language::English;
        QVERIFY(store.commit(c));ConfigStore reopened(store.path());QVERIFY(reopened.load());QCOMPARE(reopened.current(),c);
        QCOMPARE(c.resolved("c:\\apps\\CODE.exe").center.name,QString("Center"));
        QVERIFY(!c.resolved("D:/Code.exe").centerEnabled);
        c.profiles.append(profile);QVERIFY(!store.commit(c));
    }
    void sharedSvgLibraryLifecycle() {
        QTemporaryDir dir;ConfigStore store(dir.filePath("config.json"));QVERIFY(store.commit(defaultConfig()));
        QFile source(dir.filePath("original.svg"));QVERIFY(QFile::copy(QStringLiteral("fixtures/library.svg"),source.fileName()));
        QVERIFY(source.open(QIODevice::ReadOnly));const auto original=source.readAll();source.close();
        const auto id=store.importIcon(source.fileName());QVERIFY2(!id.isEmpty(),qPrintable(store.error()));QVERIFY(source.remove());
        QFile preserved(store.assetPath(id));QVERIFY(preserved.open(QIODevice::ReadOnly));QCOMPARE(preserved.readAll(),original);preserved.close();QCOMPARE(store.current().assets.size(),1);
        auto c=store.current();c.slots[0].icon={IconSource::Library,id,{}};QVERIFY(store.commit(c));
        QVERIFY(store.renameIcon(id,"Renamed"));QCOMPARE(store.current().slots[0].icon.value,id);
        QVERIFY(!store.removeIcon(id,false));QVERIFY(store.removeIcon(id,true));
        QCOMPARE(store.current().slots[0].icon.source,IconSource::Automatic);QVERIFY(!QFile::exists(store.assetPath(id)));
    }
};
QTEST_GUILESS_MAIN(WorkspaceTests)
#include "workspace_tests.moc"
