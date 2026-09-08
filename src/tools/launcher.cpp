#include "tools/launcher.h"
#include <QDesktopServices>
#include <QFileInfo>
#include <QProcess>
#include <QUrl>
namespace wheel {
bool launchTarget(const Slot& slot,QString& error) {
    error=validate(slot);
    if(!error.isEmpty()) return false;
    bool launched=false;
    if(slot.kind==ActionKind::Website) launched=QDesktopServices::openUrl(QUrl(slot.target,QUrl::StrictMode));
    else if(slot.kind==ActionKind::Application) {
        const QFileInfo file(slot.target);
        if(!file.exists()) { error=QStringLiteral("应用不存在，请在设置中重新选择。"); return false; }
        if(file.suffix().compare("exe",Qt::CaseInsensitive)==0)
            launched=QProcess::startDetached(file.absoluteFilePath(),{},file.absolutePath());
        else launched=QDesktopServices::openUrl(QUrl::fromLocalFile(file.absoluteFilePath()));
    }
    if(!launched) error=QStringLiteral("无法打开该目标，请检查地址或系统默认应用。");
    return launched;
}
}
