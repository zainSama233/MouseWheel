#include <QtTest>
#include <QTemporaryDir>
#include <QApplication>
#include <QWidget>
#include <QFileInfo>
#include <QDir>
#include <QScopeGuard>
#include <QProcess>
#include <Windows.h>
#include <shobjidl.h>
#include <wrl/client.h>
#include "platform/application_catalog.h"
#include "platform/window_context.h"
using namespace wheel;
class DiscoveryTests:public QObject {
    Q_OBJECT
private Q_SLOTS:
    void readsRealShortcutWithoutLosingArguments() {
        const HRESULT hr=CoInitializeEx(nullptr,COINIT_APARTMENTTHREADED);
        const auto cleanup=qScopeGuard([&]{if(SUCCEEDED(hr)) CoUninitialize();});
        QTemporaryDir dir; const auto path=dir.filePath(QStringLiteral("测试 应用.lnk"));
        const auto target=QDir::toNativeSeparators(QCoreApplication::applicationFilePath());
        Microsoft::WRL::ComPtr<IShellLinkW> link; Microsoft::WRL::ComPtr<IPersistFile> file;
        QVERIFY(SUCCEEDED(CoCreateInstance(CLSID_ShellLink,nullptr,CLSCTX_INPROC_SERVER,IID_PPV_ARGS(&link))));
        QVERIFY(SUCCEEDED(link->SetPath(reinterpret_cast<LPCWSTR>(target.utf16()))));
        QVERIFY(SUCCEEDED(link->SetArguments(L"--example \"参数 空格\"")));
        QVERIFY(SUCCEEDED(link.As(&file))); QVERIFY(SUCCEEDED(file->Save(reinterpret_cast<LPCWSTR>(path.utf16()),TRUE)));
        const auto apps=win::discoverApplications({dir.path()});
        const auto it=std::find_if(apps.begin(),apps.end(),[&](const auto& app){return app.path==path;});
        QVERIFY(it!=apps.end()); QCOMPARE(it->name,QStringLiteral("测试 应用"));
        QCOMPARE(it->executable.compare(QDir::fromNativeSeparators(target),Qt::CaseInsensitive),0);
        QCOMPARE(it->path,path); // Launch the shortcut itself so arguments and working directory survive.
    }
    void discoversRunningNotepad() {
        QProcess notepad; notepad.start("notepad.exe"); QVERIFY(notepad.waitForStarted());
        const auto cleanup=qScopeGuard([&]{notepad.terminate(); notepad.waitForFinished(3000);});
        QTRY_VERIFY_WITH_TIMEOUT(([&]{
            const auto windows=win::runningApplications();
            return std::any_of(windows.begin(),windows.end(),[](const auto& entry){return entry.executable.endsWith("/notepad.exe",Qt::CaseInsensitive) && !entry.name.isEmpty() && entry.path==entry.executable;});
        })(),5000);
    }
    void inspectsRealWindowAndFullscreenBounds() {
        QWidget window; window.setWindowTitle("Discovery fixture"); window.resize(400,300); window.show();
        QVERIFY(QTest::qWaitForWindowExposed(&window)); const HWND hwnd=reinterpret_cast<HWND>(window.winId());
        QCOMPARE(win::processExecutable(hwnd).compare(QCoreApplication::applicationFilePath(),Qt::CaseInsensitive),0);
        QVERIFY(!win::isFullscreen(hwnd)); window.showFullScreen();
        QTRY_VERIFY(win::isFullscreen(hwnd)); window.showNormal(); QTRY_VERIFY(!win::isFullscreen(hwnd));
        QVERIFY(win::processExecutable(nullptr).isEmpty()); QVERIFY(!win::isFullscreen(nullptr));
        const auto windows=win::runningApplications();
        QVERIFY(std::none_of(windows.begin(),windows.end(),[](const auto& app){return app.executable.compare(QCoreApplication::applicationFilePath(),Qt::CaseInsensitive)==0;}));
    }
};
QTEST_MAIN(DiscoveryTests)
#include "discovery_tests.moc"
