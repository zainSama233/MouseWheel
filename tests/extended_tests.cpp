#include <QtTest>
#include <QTemporaryDir>
#include <QTcpServer>
#include <QTcpSocket>
#include <QJsonDocument>
#include <QJsonArray>
#include <QJsonObject>
#include <QPainter>
#include <QPlainTextEdit>
#include <QDialog>
#include <QScopeGuard>
#include <Windows.h>
#include <objbase.h>
#include <exdisp.h>
#include <wrl/client.h>
#include "tools/ocr_session.h"
#include "platform/desktop_actions.h"
#include "tools/launcher.h"
using namespace wheel;
class ExtendedTests:public QObject {
    Q_OBJECT
private Q_SLOTS:
    void realArgumentsAndCommands() {
        QTemporaryDir dir; const auto exe=dir.filePath("probe.exe");
        QVERIFY(QFile::copy(QCoreApplication::applicationDirPath()+"/launch_probe.exe",exe));
        QString error; QVERIFY2(launchTarget(ApplicationAction{exe,"\"中文 空格\" \"a&b\"",dir.path(),true},error),qPrintable(error));
        QFile marker(dir.filePath("launched.txt")); QTRY_VERIFY(marker.exists()); QVERIFY(marker.open(QIODevice::ReadOnly));
        QCOMPARE(QJsonDocument::fromJson(marker.readAll()).array(),QJsonArray({QStringLiteral("中文 空格"),"a&b"})); marker.close(); QVERIFY(marker.remove());
        QVERIFY2(launchTarget(WebsiteAction{"https://example.com/a?x=1&y=2",Browser::Custom,exe},error),qPrintable(error));
        QTRY_VERIFY(marker.exists()); QVERIFY(marker.open(QIODevice::ReadOnly)); QCOMPARE(QJsonDocument::fromJson(marker.readAll()).array(),QJsonArray({"https://example.com/a?x=1&y=2"})); marker.close();
        for(auto shell:{Shell::Cmd,Shell::PowerShell}) {
            const auto path=dir.filePath(shell==Shell::Cmd?"cmd.txt":"ps.txt");
            const auto script=shell==Shell::Cmd?QString("echo hello>\"%1\"").arg(QDir::toNativeSeparators(path)):QString("[IO.File]::WriteAllText('%1','hello')").arg(path);
            QVERIFY2(launchTarget(CommandAction{shell,script,dir.path(),true},error),qPrintable(error));
            QTRY_VERIFY(QFile::exists(path)); QFile output(path); QVERIFY(output.open(QIODevice::ReadOnly)); QVERIFY(output.readAll().contains("hello"));
        }
    }
    void opensRealFolder() {
        QTemporaryDir dir; QString error;
        const HRESULT initialized=CoInitializeEx(nullptr,COINIT_APARTMENTTHREADED);QVERIFY(SUCCEEDED(initialized));
        const auto cleanup=qScopeGuard([]{CoUninitialize();});
        Microsoft::WRL::ComPtr<IShellWindows> windows;QVERIFY(SUCCEEDED(CoCreateInstance(CLSID_ShellWindows,nullptr,CLSCTX_LOCAL_SERVER,IID_PPV_ARGS(&windows))));
        const auto find=[&] {
            Microsoft::WRL::ComPtr<IWebBrowser2> result;long count=0;windows->get_Count(&count);
            for(long i=0;i<count;++i) {VARIANT index{};index.vt=VT_I4;index.lVal=i;Microsoft::WRL::ComPtr<IDispatch> dispatch;
                if(FAILED(windows->Item(index,&dispatch)) || !dispatch)continue;Microsoft::WRL::ComPtr<IWebBrowser2> browser;if(FAILED(dispatch.As(&browser)))continue;
                BSTR url=nullptr;if(SUCCEEDED(browser->get_LocationURL(&url))) {const auto path=QUrl(QString::fromWCharArray(url)).toLocalFile();SysFreeString(url);if(QDir::cleanPath(path).compare(QDir::cleanPath(dir.path()),Qt::CaseInsensitive)==0) return browser;}
            } return result;
        };
        QVERIFY(!find());QVERIFY2(launchTarget(FolderAction{FolderLocation::Path,dir.path()},error),qPrintable(error));
        QTRY_VERIFY_WITH_TIMEOUT(find(),5000);const auto window=find();QVERIFY(window);QVERIFY(SUCCEEDED(window->Quit()));QTest::qWait(150);
    }
    void windowOperationsOnRealWindow() {
        QWidget target;target.resize(350,230);target.show();QVERIFY(QTest::qWaitForWindowExposed(&target));
        const auto handle=reinterpret_cast<HWND>(target.winId()); QString error; win::KeyState keys{};
        QVERIFY(win::executeDesktopAction(WindowAction{WindowOperation::Topmost},handle,keys,error));
        QVERIFY(GetWindowLongPtrW(handle,GWL_EXSTYLE)&WS_EX_TOPMOST);
        QVERIFY(win::executeDesktopAction(WindowAction{WindowOperation::Topmost},handle,keys,error));
        QVERIFY(!(GetWindowLongPtrW(handle,GWL_EXSTYLE)&WS_EX_TOPMOST));
        QVERIFY(win::executeDesktopAction(WindowAction{WindowOperation::Opacity,70},handle,keys,error));
        BYTE alpha=0;DWORD flags=0;QVERIFY(GetLayeredWindowAttributes(handle,nullptr,&alpha,&flags));QCOMPARE(alpha,BYTE(70*255/100));
        QVERIFY(win::executeDesktopAction(WindowAction{WindowOperation::TileLeft},handle,keys,error));
        RECT rect{};GetWindowRect(handle,&rect);MONITORINFO monitor{sizeof(monitor)};GetMonitorInfoW(MonitorFromWindow(handle,MONITOR_DEFAULTTONEAREST),&monitor);
        QCOMPARE(rect.left,monitor.rcWork.left);QCOMPARE(rect.right,(monitor.rcWork.left+monitor.rcWork.right)/2);
    }
    void remoteOcr_data() { QTest::addColumn<bool>("ai"); QTest::newRow("HTTP")<<false;QTest::newRow("AI")<<true; }
    void remoteOcr() {
        QFETCH(bool,ai);QTcpServer server;QVERIFY(server.listen(QHostAddress::LocalHost)); QByteArray received;
        connect(&server,&QTcpServer::newConnection,this,[&]{auto* socket=server.nextPendingConnection();
            connect(socket,&QTcpSocket::readyRead,socket,[&,socket]{received+=socket->readAll();const int split=received.indexOf("\r\n\r\n");if(split<0)return;
                const auto headers=received.left(split);QRegularExpression expression("Content-Length: (\\d+)",QRegularExpression::CaseInsensitiveOption);const auto match=expression.match(QString::fromLatin1(headers));
                if(!match.hasMatch() || received.size()-split-4<match.captured(1).toInt())return;
                const auto body=ai?QByteArray(R"({"choices":[{"message":{"content":"recognized text"}}]})"):QByteArray(R"({"result":{"text":"recognized text"}})");
                socket->write("HTTP/1.1 200 OK\r\nContent-Type: application/json\r\nContent-Length: "+QByteArray::number(body.size())+"\r\nConnection: close\r\n\r\n"+body);socket->disconnectFromHost();
            });});
        OcrSession session;QSignalSpy completed(&session,&OcrSession::completed);QImage image(100,80,QImage::Format_RGB32);image.fill(Qt::white);
        session.recognize(image,OcrAction{ai?OcrProvider::Ai:OcrProvider::Http,QString("http://127.0.0.1:%1/ocr").arg(server.serverPort()),"test-key","test-model","result.text"},Theme::Dark);
        QTRY_COMPARE(completed.size(),1);QCOMPARE(completed[0][0].toString(),QString("recognized text"));QVERIFY(completed[0][1].toString().isEmpty());
        QVERIFY(received.contains("Bearer test-key"));const auto request=QJsonDocument::fromJson(received.mid(received.indexOf("\r\n\r\n")+4)).object();
        if(ai) QVERIFY(request["messages"].isArray());else QVERIFY(!QByteArray::fromBase64(request["image"].toString().toLatin1()).isEmpty());session.cancel();
    }
    void localOcrRealImage() {
        QImage image(700,130,QImage::Format_RGB32);image.fill(Qt::white);QPainter painter(&image);painter.setPen(Qt::black);painter.setFont(QFont("Arial",36));painter.drawText(image.rect(),Qt::AlignCenter,"HELLO WORLD 123");painter.end();
        OcrSession session;QSignalSpy completed(&session,&OcrSession::completed);session.recognize(image,{},Theme::Light);
        QTRY_COMPARE_WITH_TIMEOUT(completed.size(),1,30000);QVERIFY2(completed[0][1].toString().isEmpty(),qPrintable(completed[0][1].toString()));
        QVERIFY2(completed[0][0].toString().contains("HELLO"),qPrintable(completed[0][0].toString()));session.cancel();
    }
    void remoteFailureIsReported() {
        QTcpServer server;QVERIFY(server.listen(QHostAddress::LocalHost));
        connect(&server,&QTcpServer::newConnection,this,[&]{auto* socket=server.nextPendingConnection();connect(socket,&QTcpSocket::readyRead,socket,[socket]{socket->readAll();socket->write("HTTP/1.1 401 Unauthorized\r\nContent-Length: 0\r\nConnection: close\r\n\r\n");socket->disconnectFromHost();});});
        OcrSession session;QSignalSpy completed(&session,&OcrSession::completed);QImage image(20,20,QImage::Format_RGB32);image.fill(Qt::white);
        session.recognize(image,OcrAction{OcrProvider::Http,QString("http://127.0.0.1:%1/ocr").arg(server.serverPort()),{},{},"text"},Theme::Light);
        QTRY_COMPARE(completed.size(),1);QVERIFY(completed[0][0].toString().isEmpty());QVERIFY(completed[0][1].toString().contains("401"));
    }
    void cancellingRemoteOcrIgnoresResponse() {
        QTcpServer server;QVERIFY(server.listen(QHostAddress::LocalHost));OcrSession session;QSignalSpy completed(&session,&OcrSession::completed);
        QImage image(20,20,QImage::Format_RGB32);image.fill(Qt::white);
        session.recognize(image,OcrAction{OcrProvider::Http,QString("http://127.0.0.1:%1/ocr").arg(server.serverPort()),{},{},"text"},Theme::Light);
        session.cancel();QTest::qWait(100);QCOMPARE(completed.size(),0);
    }
};
QTEST_MAIN(ExtendedTests)
#include "extended_tests.moc"
