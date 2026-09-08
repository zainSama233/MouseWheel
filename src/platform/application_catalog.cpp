#include "platform/application_catalog.h"
#include "platform/window_context.h"
#include "platform/program_icon.h"
#include <QDirIterator>
#include <QFileInfo>
#include <QSettings>
#include <QSet>
#include <Windows.h>
#include <shlobj.h>
#include <dwmapi.h>
namespace wheel::win {
QStringList applicationShortcutRoots() {
    QStringList roots;
    for(const auto* id:{&FOLDERID_Programs,&FOLDERID_CommonPrograms,&FOLDERID_Desktop,&FOLDERID_PublicDesktop}) {
        PWSTR path=nullptr;
        if(SUCCEEDED(SHGetKnownFolderPath(*id,KF_FLAG_DONT_VERIFY,nullptr,&path))) roots.append(QDir::fromNativeSeparators(QString::fromWCharArray(path)));
        CoTaskMemFree(path);
    }
    return roots;
}
QList<ApplicationEntry> discoverApplications(const QStringList& roots,const std::atomic_bool* cancelled) {
    QList<ApplicationEntry> result; QSet<QString> seen;
    for(const auto& root:roots) {
        QDirIterator files(root,{"*.lnk"},QDir::Files,QDirIterator::Subdirectories);
        while(files.hasNext()) {
            if(cancelled && cancelled->load()) return {};
            const auto path=files.next(); const auto identity=QDir::cleanPath(path).toCaseFolded();
            if(seen.contains(identity)) continue;
            seen.insert(identity);
            const auto executable=readShortcut(path).target;
            if(!executable.endsWith(".exe",Qt::CaseInsensitive)) continue;
            result.append({QFileInfo(path).completeBaseName(),path,executable});
        }
    }
    QSet<QString> executables;
    for(const auto& item:result) executables.insert(item.executable.toCaseFolded());
    for(auto format:{QSettings::Registry64Format,QSettings::Registry32Format}) {
        for(const auto& hive:{QStringLiteral("HKEY_CURRENT_USER"),QStringLiteral("HKEY_LOCAL_MACHINE")}) {
            QSettings registry(hive+"\\Software\\Microsoft\\Windows\\CurrentVersion\\App Paths",format);
            for(const auto& key:registry.childGroups()) {
                if(cancelled && cancelled->load()) return {};
                auto path=QDir::fromNativeSeparators(registry.value(key+"/.").toString().trimmed());
                if(path.startsWith('"') && path.endsWith('"')) path=path.mid(1,path.size()-2);
                if(!path.endsWith(".exe",Qt::CaseInsensitive) || !QFileInfo::exists(path) || executables.contains(path.toCaseFolded())) continue;
                executables.insert(path.toCaseFolded()); result.append({QFileInfo(path).completeBaseName(),path,path});
            }
        }
    }
    std::sort(result.begin(),result.end(),[](const auto& a,const auto& b){return QString::localeAwareCompare(a.name,b.name)<0;});
    return result;
}
QList<ApplicationEntry> runningApplications() {
    QList<ApplicationEntry> result;
    EnumWindows([](HWND window,LPARAM data)->BOOL {
        DWORD pid=0; GetWindowThreadProcessId(window,&pid);
        if(!IsWindowVisible(window) || pid==GetCurrentProcessId() || window==GetShellWindow() ||
           (GetWindowLongPtrW(window,GWL_EXSTYLE)&WS_EX_TOOLWINDOW)) return TRUE;
        DWORD cloaked=0; DwmGetWindowAttribute(window,DWMWA_CLOAKED,&cloaked,sizeof(cloaked));
        if(cloaked) return TRUE;
        wchar_t title[1024]{}; if(!GetWindowTextW(window,title,1024)) return TRUE;
        const auto executable=processExecutable(window); if(executable.isEmpty()) return TRUE;
        auto* entries=reinterpret_cast<QList<ApplicationEntry>*>(data);
        entries->append({QString::fromWCharArray(title),executable,executable}); return TRUE;
    },reinterpret_cast<LPARAM>(&result));
    return result;
}
}
