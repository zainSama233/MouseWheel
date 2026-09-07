#include "app.h"
#include <QApplication>
#include <QCommandLineParser>
#include <QDir>
#include <QFileInfo>
#include <QLockFile>
#include <QMessageBox>
#include <QTimer>
#include <Windows.h>
int main(int argc, char** argv) {
    SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);
    QApplication application(argc,argv);
    application.setApplicationName("MouseWheel");
    application.setApplicationVersion(QStringLiteral(MOUSEWHEEL_VERSION));
    application.setQuitOnLastWindowClosed(false);
    QCommandLineParser parser;
    parser.setApplicationDescription(QStringLiteral("鼠标快捷强化"));
    parser.addHelpOption(); parser.addVersionOption();
    parser.addOption({"settings",QStringLiteral("打开设置")});
    parser.addOption({"config",QStringLiteral("指定配置文件，用于独立验证"),"path"});
    parser.addOption({"smoke-test",QStringLiteral("启动设置并在两秒后退出")});
    parser.process(application);
    const auto path = parser.isSet("config") ? QFileInfo(parser.value("config")).absoluteFilePath() :
                      QCoreApplication::applicationDirPath() + "/config.json";
    QLockFile lock(path + ".lock");
    lock.setStaleLockTime(0);
    if (!lock.tryLock()) {
        QMessageBox::warning(nullptr,QStringLiteral("鼠标快捷强化"),
            QStringLiteral("程序已经运行，或配置目录不可写。请检查系统托盘与目录权限。"));
        return 1;
    }
    wheel::App app(path);
    app.start(parser.isSet("settings") || parser.isSet("smoke-test"));
    if (parser.isSet("smoke-test")) QTimer::singleShot(2000,&application,&QCoreApplication::quit);
    return application.exec();
}
