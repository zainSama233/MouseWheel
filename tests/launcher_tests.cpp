#include <QtTest>
#include <QScopeGuard>
#include <Windows.h>
#include <shobjidl.h>
#include <wrl/client.h>
#include <QDesktopServices>
#include <QTemporaryDir>
#include "tools/launcher.h"
using namespace wheel;
class LauncherTests final:public QObject {
    Q_OBJECT
    QUrl opened_;
public Q_SLOTS:
    void opened(QUrl url) { opened_=url; }
private Q_SLOTS:
    void webTargetUsesDesktopServices() {
        QDesktopServices::setUrlHandler("https",this,"opened");
        const auto cleanup=qScopeGuard([]{QDesktopServices::unsetUrlHandler("https");});
        QString error;
        QVERIFY(launchTarget(WebsiteAction{"https://example.com/path?a=1&b=2"},error));
        QCOMPARE(opened_,QUrl("https://example.com/path?a=1&b=2"));
        opened_={}; QVERIFY(!launchTarget(WebsiteAction{"javascript:alert(1)"},error)); QVERIFY(opened_.isEmpty());
    }
    void realApplicationWithSpaces() {
        QTemporaryDir dir; const auto executable=dir.filePath(QStringLiteral("应用 空格 &.exe"));
        QVERIFY(QFile::copy(QCoreApplication::applicationDirPath()+"/launch_probe.exe",executable));
        QString error; QVERIFY2(launchTarget(ApplicationAction{executable},error),qPrintable(error));
        QTRY_VERIFY(QFile::exists(dir.filePath("launched.txt")));
        const auto initialized=CoInitializeEx(nullptr,COINIT_APARTMENTTHREADED);
        QVERIFY(SUCCEEDED(initialized)); const auto uninitialize=qScopeGuard([]{CoUninitialize();});
        Microsoft::WRL::ComPtr<IShellLinkW> link;
        QVERIFY(SUCCEEDED(CoCreateInstance(CLSID_ShellLink,nullptr,CLSCTX_INPROC_SERVER,IID_PPV_ARGS(&link))));
        QVERIFY(SUCCEEDED(link->SetPath(reinterpret_cast<LPCWSTR>(executable.utf16()))));
        QVERIFY(SUCCEEDED(link->SetWorkingDirectory(reinterpret_cast<LPCWSTR>(dir.path().utf16()))));
        Microsoft::WRL::ComPtr<IPersistFile> file; QVERIFY(SUCCEEDED(link.As(&file)));
        const auto shortcut=dir.filePath(QStringLiteral("应用 快捷方式.lnk"));
        QVERIFY(SUCCEEDED(file->Save(reinterpret_cast<LPCWSTR>(shortcut.utf16()),TRUE)));
        QVERIFY(QFile::remove(dir.filePath("launched.txt")));
        QVERIFY2(launchTarget(ApplicationAction{shortcut},error),qPrintable(error));
        QTRY_VERIFY(QFile::exists(dir.filePath("launched.txt")));
        QVERIFY(!launchTarget(ApplicationAction{dir.filePath("missing.exe")},error)); QVERIFY(!error.isEmpty());
    }
};
QTEST_MAIN(LauncherTests)
#include "launcher_tests.moc"
