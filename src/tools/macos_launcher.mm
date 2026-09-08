#include "tools/launcher.h"
#include <QDesktopServices>
#include <QFileInfo>
#include <QProcess>
#include <QStandardPaths>
#include <QDir>
#include <QUrl>
namespace wheel {
bool launchTarget(const Action& action,QString& error) {
    error=validate(action);if(!error.isEmpty())return false;
    QString program;QStringList args;QString directory;
    if(const auto* app=std::get_if<ApplicationAction>(&action)) {
        if(!QFileInfo::exists(app->path)){error=QStringLiteral("应用或文件不存在。");return false;}
        directory=app->directory;
        if(app->path.endsWith(".app")){program="/usr/bin/open";args={"-a",app->path};if(!app->arguments.isEmpty())args<<"--args"<<QProcess::splitCommand(app->arguments);}
        else if(QFileInfo(app->path).isExecutable()){return QProcess::startDetached(app->path,QProcess::splitCommand(app->arguments),directory);}
        else return QDesktopServices::openUrl(QUrl::fromLocalFile(app->path));
    } else if(const auto* web=std::get_if<WebsiteAction>(&action)) {
        if(web->browser==Browser::Default)return QDesktopServices::openUrl(QUrl(web->url));
        const QStringList browsers{"","Google Chrome","Microsoft Edge","Firefox"};
        program="/usr/bin/open";args={"-a",web->browser==Browser::Custom?web->executable:browsers[int(web->browser)],web->url};
    } else if(const auto* folder=std::get_if<FolderAction>(&action)) {
        QString path=folder->path;
        switch(folder->location) {
        case FolderLocation::Path:break;
        case FolderLocation::Desktop:path=QStandardPaths::writableLocation(QStandardPaths::DesktopLocation);break;
        case FolderLocation::Downloads:path=QStandardPaths::writableLocation(QStandardPaths::DownloadLocation);break;
        case FolderLocation::Documents:path=QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation);break;
        case FolderLocation::Pictures:path=QStandardPaths::writableLocation(QStandardPaths::PicturesLocation);break;
        case FolderLocation::Home:path=QDir::homePath();break;
        case FolderLocation::Computer:path="/Volumes";break;
        case FolderLocation::RecycleBin:path=QDir::homePath()+"/.Trash";break;
        }return QDesktopServices::openUrl(QUrl::fromLocalFile(path));
    } else if(const auto* command=std::get_if<CommandAction>(&action)) {
        if(command->shell!=Shell::Zsh && command->shell!=Shell::PowerShell){error=QStringLiteral("此终端仅适用于 Windows，请选择 Zsh。");return false;}
        program=command->shell==Shell::Zsh?"/bin/zsh":QStandardPaths::findExecutable("pwsh");directory=command->directory;
        if(program.isEmpty()){error=QStringLiteral("未安装所选终端。");return false;}
        args=command->shell==Shell::Zsh?QStringList{"-lc",command->script}:QStringList{"-NoProfile","-Command",command->script};
        if(!command->hidden) {
            const auto quote=[](QString value){return "'"+value.replace("'","'\\''")+"'";};
            QString script=directory.isEmpty()?QString{}:"cd "+quote(directory)+" && ";script+=quote(program);for(const auto& arg:args)script+=" "+quote(arg);
            program="/usr/bin/osascript";args={"-e","on run argv","-e","tell application \"Terminal\" to do script (item 1 of argv)","-e","end run",script};
        }
        QProcess process;process.setProgram(program);process.setArguments(args);process.setWorkingDirectory(directory);if(process.startDetached())return true;error=process.errorString();return false;
    } else {error=QStringLiteral("此操作不能通过应用启动器执行。");return false;}
    QProcess process;process.setProgram(program);process.setArguments(args);process.setWorkingDirectory(directory);process.start();
    if(!process.waitForFinished(5000)){error=process.errorString();return false;}
    if(process.exitCode()==0)return true;error=QString::fromUtf8(process.readAllStandardError());return false;
}
}
