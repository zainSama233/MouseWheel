#include <QtTest>
#include <QTcpServer>
#include <QTcpSocket>
#include <QBuffer>
#include <QLineEdit>
#include <QComboBox>
#include <QTemporaryDir>
#include "tools/website_icon.h"
#include "ui/action_icons.h"
#include "ui/slot_editor.h"
#include "core/image_asset.h"
#include "config/config_store.h"
using namespace wheel;
class IconServer:public QTcpServer {
public:
    QHash<QByteArray,QByteArray> bodies;
    QList<QByteArray> requests;
    IconServer() {
        listen(QHostAddress::LocalHost);
        connect(this,&QTcpServer::newConnection,this,[this]{auto* socket=nextPendingConnection();
            auto input=std::make_shared<QByteArray>();
            connect(socket,&QTcpSocket::readyRead,socket,[this,socket,input]{
                input->append(socket->readAll()); if(!input->contains("\r\n\r\n")) return;
                const auto path=input->split(' ').value(1); requests.append(path); if(path=="/wait") return;
                const auto body=bodies.value(path); const auto status=bodies.contains(path)?"200 OK":"404 Not Found";
                socket->write(QByteArray("HTTP/1.1 ")+status+"\r\nContent-Length: "+QByteArray::number(body.size())+"\r\nConnection: close\r\n\r\n"+body);
                socket->disconnectFromHost();
            }); connect(socket,&QTcpSocket::disconnected,socket,&QObject::deleteLater);
        });
    }
    QUrl url(const QString& path) const {return QUrl(QString("http://127.0.0.1:%1%2").arg(serverPort()).arg(path));}
};
class IconTests:public QObject {
    Q_OBJECT
    QByteArray png_;
private Q_SLOTS:
    void initTestCase() {QImage image(64,64,QImage::Format_ARGB32);image.fill(Qt::red);QBuffer buffer(&png_);buffer.open(QIODevice::WriteOnly);QVERIFY(image.save(&buffer,"PNG"));}
    void systemActionsHaveDistinctVectorIcons() {
        const QStringList expected{"lock-keyhole","volume-2","volume-1","volume-x","circle-play","skip-forward","skip-back","panels-top-left","panel-left-close","panel-right-close","square-plus","monitor-x","monitor"};
        QCOMPARE(expected.size(),int(SystemOperation::ShowDesktop)+1);
        for(int i=0;i<expected.size();++i) {
            Slot slot{QStringLiteral("System"),SystemAction{SystemOperation(i)}};
            slot.icon={IconSource::Automatic,{},{}};
            QCOMPARE(suggestedIcon(slot.action).value,expected[i]);
            QVERIFY(std::any_of(builtinIcons().begin(),builtinIcons().end(),[&](const auto& icon){return icon.id==expected[i];}));
            for(const QColor color:{QColor("#253047"),QColor("#554a35"),QColor("#e5e9f0")}) {
                const auto image=actionIcon(slot,color).pixmap(48,48).toImage();
                QVERIFY(!image.isNull()); bool visible=false;
                for(int y=0;y<image.height();++y) for(int x=0;x<image.width();++x) visible|=image.pixelColor(x,y).alpha()>0;
                QVERIFY(visible);
            }
        }
    }
    void systemEditorKeepsAutomaticAndManualIconsIndependent() {
        SlotEditor editor(0); Slot slot{QStringLiteral("System"),SystemAction{}};
        slot.icon={IconSource::Automatic,{},{}}; editor.setSlot(slot);
        QComboBox* operation=nullptr;
        for(auto* combo:editor.findChildren<QComboBox*>()) if(combo->count()==13 && combo->itemText(0)==QStringLiteral("锁屏")) operation=combo;
        QVERIFY(operation); QSignalSpy edited(&editor,&SlotEditor::edited);
        operation->setCurrentIndex(int(SystemOperation::VolumeUp));
        QVERIFY(!edited.isEmpty()); QCOMPARE(suggestedIcon(editor.slot().action).value,QString("volume-2"));
        QCOMPARE(editor.slot().icon.source,IconSource::Automatic);
        slot=editor.slot();slot.icon={IconSource::Builtin,"camera",{}};editor.setSlot(slot);
        operation->setCurrentIndex(int(SystemOperation::Lock));
        QCOMPARE(editor.slot().icon,slot.icon);
    }
    void decodesIcoAndSvg() {
        const auto image=QImage::fromData(png_); QByteArray ico; QBuffer buffer(&ico); buffer.open(QIODevice::WriteOnly);
        QVERIFY(image.save(&buffer,"ICO")); QByteArray normalized; QString error;
        QVERIFY2(importImageAsset(ico,normalized,error),qPrintable(error)); QVERIFY(!decodeImageAsset(normalized).isNull());
        const QByteArray svg=R"(<svg xmlns="http://www.w3.org/2000/svg" width="64" height="64"><circle cx="32" cy="32" r="30" fill="red"/></svg>)";
        QVERIFY2(importImageAsset(svg,normalized,error),qPrintable(error)); QVERIFY(!decodeImageAsset(normalized).isNull());
    }
    void liveWebsite() {
        if(!qEnvironmentVariableIsSet("WHEEL_LIVE_ICON_TEST")) QSKIP("Opt-in live network verification");
        WebsiteIcon loader; QSignalSpy ready(&loader,&WebsiteIcon::ready); loader.load(QUrl("https://www.python.org"));
        QTRY_COMPARE_WITH_TIMEOUT(ready.size(),1,30000);
        QVERIFY2(ready[0][2].toString().isEmpty(),qPrintable(ready[0][2].toString()));
        QVERIFY(!decodeImageAsset(ready[0][1].toByteArray()).isNull());
    }
    void discoversRelativeIconAndCachesInConfig() {
        IconServer server; server.bodies["/page"]="<html><head><link href='/brand.png?a=1&amp;b=2' rel='shortcut icon'></head></html>";
        server.bodies["/brand.png?a=1&b=2"]=png_;
        SlotEditor editor(0); editor.setSlot({});
        editor.findChild<QComboBox*>("slot-kind-0")->setCurrentIndex(int(ActionKind::Website));
        auto* url=editor.findChild<QLineEdit*>("slot-target-0"); url->setText(server.url("/page").toString());
        QTRY_VERIFY_WITH_TIMEOUT(!editor.slot().icon.image.isEmpty(),5000);
        QCOMPARE(editor.slot().icon.source,IconSource::Automatic); QVERIFY(!decodeImageAsset(editor.slot().icon.image).isNull());
        QCOMPARE(server.requests,QList<QByteArray>({"/page","/brand.png?a=1&b=2"}));
        QTemporaryDir dir;ConfigStore store(dir.filePath("config.json"));auto config=defaultConfig(); config.slots[2]=editor.slot(); QVERIFY(store.commit(config));
        ConfigStore reopened(store.path());QVERIFY(reopened.load());QCOMPARE(reopened.current(),config);
    }
    void missingLinkUsesFaviconAndFailureIsReported() {
        IconServer server;server.bodies["/page"]="<html></html>";server.bodies["/favicon.ico"]=png_;
        WebsiteIcon loader;QSignalSpy ready(&loader,&WebsiteIcon::ready);loader.load(server.url("/page"));
        QTRY_COMPARE(ready.size(),1);QVERIFY(!ready[0][1].toByteArray().isEmpty());
        server.bodies.remove("/favicon.ico");ready.clear();loader.load(server.url("/missing"));
        QTRY_COMPARE(ready.size(),1);QVERIFY(!ready[0][2].toString().isEmpty());
    }
    void cancelledRequestCannotReplaceNewIcon() {
        IconServer server;server.bodies["/new"]="";server.bodies["/favicon.ico"]=png_;
        WebsiteIcon loader;QSignalSpy ready(&loader,&WebsiteIcon::ready);loader.load(server.url("/wait"));
        QTRY_VERIFY(server.requests.contains("/wait"));loader.load(server.url("/new"));
        QTRY_COMPARE(ready.size(),1);QCOMPARE(ready[0][0].toUrl(),server.url("/new"));
        SlotEditor editor(0);editor.setSlot(Slot{"Web",WebsiteAction{server.url("/wait").toString()}});
        editor.findChild<QLineEdit*>("slot-target-0")->setText(server.url("/wait?second").toString());
        editor.findChild<QComboBox*>("slot-icon-source-0")->setCurrentIndex(int(IconSource::Builtin));
        QTest::qWait(450);QCOMPARE(editor.slot().icon.source,IconSource::Builtin);QVERIFY(editor.slot().icon.image.isEmpty());
    }
};
QTEST_MAIN(IconTests)
#include "icon_tests.moc"
