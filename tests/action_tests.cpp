#include <QtTest>
#include <QTemporaryDir>
#include <QBuffer>
#include <QImage>
#include "config/action_codec.h"
#include "config/config_store.h"
using namespace wheel;
class ActionTests: public QObject {
    Q_OBJECT
private Q_SLOTS:
    void independentAppearanceRoundTrip() {
        QTemporaryDir dir; ConfigStore store(dir.filePath("config.json"));
        auto c=defaultConfig(); auto& slot=c.slots[2];
        slot.name="Web"; slot.action=WebsiteAction{"https://example.com",Browser::Firefox,{}};
        slot.icon={IconSource::Builtin,"copy",{}}; slot.showLabel=false;
        QVERIFY2(store.commit(c),qPrintable(store.error()));
        slot.action=ApplicationAction{"C:/Windows/notepad.exe","\"a b.txt\"",{},true};
        QVERIFY(store.commit(c));
        ConfigStore read(store.path()); QVERIFY(read.load()); QCOMPARE(read.current(),c);
        QCOMPARE(read.current().slots[2].icon.value,QString("copy"));
    }
    void everyActionRoundTrips() {
        QTemporaryDir dir; ConfigStore store(dir.filePath("config.json"));
        const QList<Action> actions{Shortcut{Qt::Key_Pause,0},ScreenshotAction{},AnnotationAction{},
            ApplicationAction{"C:/Windows/notepad.exe","arg","C:/Windows",false},
            WebsiteAction{"https://example.com",Browser::Custom,"C:/browser.exe"},
            FolderAction{FolderLocation::Path,"C:/Windows"},CommandAction{Shell::Wsl,"printf hello","C:/Windows",false},CommandAction{Shell::Zsh,"printf hello",{},true},
            OcrAction{OcrProvider::Ai,"https://example.com/recognize","key","vision","text"},
            WindowAction{WindowOperation::Opacity,72},SystemAction{SystemOperation::NewDesktop}};
        for(const auto& action:actions) {
            auto c=defaultConfig(); c.slots[0]=Slot{"Action",action};
            QVERIFY2(store.commit(c),qPrintable(store.error())); ConfigStore read(store.path()); QVERIFY(read.load()); QCOMPARE(read.current(),c);
        }
        auto c=defaultConfig(); QImage image(32,32,QImage::Format_RGB32);image.fill(Qt::red);QByteArray png;QBuffer buffer(&png);buffer.open(QIODevice::WriteOnly);QVERIFY(image.save(&buffer,"PNG"));
        c.slots[0].icon={IconSource::Image,{},png};QVERIFY(store.commit(c));ConfigStore read(store.path());QVERIFY(read.load());QCOMPARE(read.current(),c);
        auto encoded=encodeSlot(c.slots[0]);encoded["showLabel"]="false";QVERIFY(!decodeSlot(encoded));
    }
    void allActionsValidate() {
        for(Action action:{Action{FolderAction{FolderLocation::Downloads,{}}},
            Action{CommandAction{Shell::PowerShell,"Write-Output 'hello'",{},true}},
            Action{OcrAction{}},Action{WindowAction{}},Action{SystemAction{}}}) {
            Slot slot{"Action",action}; QVERIFY2(validate(slot).isEmpty(),qPrintable(validate(slot)));
        }
        Slot invalid{"Bad",WebsiteAction{"https://example.com",Browser::Custom,{}}};
        QVERIFY(!validate(invalid).isEmpty());
        Slot remote{"OCR",OcrAction{OcrProvider::Http,"http://127.0.0.1:8123/ocr",{}, {},"text"}};
        QVERIFY(validate(remote).isEmpty());
        QVERIFY(supportedKey(Qt::Key_Pause)); QVERIFY(supportedKey(Qt::Key_Cancel));
    }
};
QTEST_GUILESS_MAIN(ActionTests)
#include "action_tests.moc"
