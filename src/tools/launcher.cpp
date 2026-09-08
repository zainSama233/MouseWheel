#include <QCoreApplication>
#include "tools/launcher.h"
#include <QDesktopServices>
#include <QFileInfo>
#include <QDir>
#include <QProcess>
#include <QSettings>
#include <QStandardPaths>
#include <QUrl>
#include <QScopeGuard>
#include <Windows.h>
#include <shellapi.h>
#include <shlobj.h>
#include <shldisp.h>
#include <exdisp.h>
#include <wrl/client.h>
namespace wheel {
namespace {
bool openFile(const QString& path,const QString& arguments,const QString& directory,bool normal,QString& error) {
    HANDLE token=nullptr; TOKEN_ELEVATION elevation{}; DWORD size=0;
    if(!OpenProcessToken(GetCurrentProcess(),TOKEN_QUERY,&token)) { error=QCoreApplication::translate("MouseWheel","无法读取启动权限。"); return false; }
    const auto tokenCleanup=qScopeGuard([&]{CloseHandle(token);});
    if(!GetTokenInformation(token,TokenElevation,&elevation,sizeof(elevation),&size)) { error=QCoreApplication::translate("MouseWheel","无法读取启动权限。"); return false; }
    if(normal && elevation.TokenIsElevated) {
        const HRESULT initialized=CoInitializeEx(nullptr,COINIT_APARTMENTTHREADED);
        const auto cleanup=qScopeGuard([&]{if(SUCCEEDED(initialized)) CoUninitialize();});
        using Microsoft::WRL::ComPtr;
        ComPtr<IShellWindows> windows; ComPtr<IDispatch> dispatch, background, application;
        ComPtr<IServiceProvider> provider; ComPtr<IShellBrowser> browser; ComPtr<IShellView> view;
        ComPtr<IShellFolderViewDual> folder; ComPtr<IShellDispatch2> shell;
        VARIANT empty{}; long handle=0;
        HRESULT hr=CoCreateInstance(CLSID_ShellWindows,nullptr,CLSCTX_LOCAL_SERVER,IID_PPV_ARGS(&windows));
        if(SUCCEEDED(hr)) hr=windows->FindWindowSW(&empty,&empty,SWC_DESKTOP,&handle,SWFO_NEEDDISPATCH,&dispatch);
        if(SUCCEEDED(hr)) hr=dispatch.As(&provider);
        if(SUCCEEDED(hr)) hr=provider->QueryService(SID_STopLevelBrowser,IID_PPV_ARGS(&browser));
        if(SUCCEEDED(hr)) hr=browser->QueryActiveShellView(&view);
        if(SUCCEEDED(hr)) hr=view->GetItemObject(SVGIO_BACKGROUND,IID_PPV_ARGS(&background));
        if(SUCCEEDED(hr)) hr=background.As(&folder);
        if(SUCCEEDED(hr)) hr=folder->get_Application(&application);
        if(SUCCEEDED(hr)) hr=application.As(&shell);
        BSTR file=SysAllocString(reinterpret_cast<const wchar_t*>(path.utf16()));
        VARIANT args{},dir{},verb{},show{};
        args.vt=dir.vt=verb.vt=VT_BSTR; show.vt=VT_I4; show.lVal=SW_SHOWNORMAL;
        args.bstrVal=SysAllocString(reinterpret_cast<const wchar_t*>(arguments.utf16()));
        dir.bstrVal=SysAllocString(reinterpret_cast<const wchar_t*>(directory.utf16())); verb.bstrVal=SysAllocString(L"open");
        if(SUCCEEDED(hr)) hr=shell->ShellExecute(file,args,dir,verb,show);
        SysFreeString(file); VariantClear(&args); VariantClear(&dir); VariantClear(&verb);
        if(FAILED(hr)) { error=QCoreApplication::translate("MouseWheel","无法通过桌面以普通权限启动（%1）。").arg(quint32(hr),0,16); return false; }
        return true;
    }
    SHELLEXECUTEINFOW info{}; info.cbSize=sizeof(info); info.fMask=SEE_MASK_FLAG_NO_UI;
    info.lpVerb=L"open"; info.lpFile=reinterpret_cast<LPCWSTR>(path.utf16());
    info.lpParameters=reinterpret_cast<LPCWSTR>(arguments.utf16());
    info.lpDirectory=directory.isEmpty()?nullptr:reinterpret_cast<LPCWSTR>(directory.utf16()); info.nShow=SW_SHOWNORMAL;
    if(!ShellExecuteExW(&info)) { error=QCoreApplication::translate("MouseWheel","无法打开目标（%1）。").arg(GetLastError()); return false; }
    return true;
}
}
bool launchTarget(const Action& action,QString& error) {
    error=validate(action); if(!error.isEmpty()) return false;
    if(const auto* a=std::get_if<ApplicationAction>(&action)) {
        const QFileInfo file(a->path);
        if(!file.exists() || file.isDir()) { error=QCoreApplication::translate("MouseWheel","文件不存在，请重新选择。"); return false; }
        return openFile(file.absoluteFilePath(),a->arguments,a->directory.isEmpty()?file.absolutePath():a->directory,a->normalUser,error);
    }
    if(const auto* a=std::get_if<WebsiteAction>(&action)) {
        const QUrl url(a->url,QUrl::StrictMode);
        if(a->browser==Browser::Default) {
            if(QDesktopServices::openUrl(url)) return true;
        } else {
            QString executable=a->executable;
            if(a->browser!=Browser::Custom) {
                const QString name=a->browser==Browser::Chrome?"chrome.exe":a->browser==Browser::Edge?"msedge.exe":"firefox.exe";
                for(const auto& root:{QString("HKEY_CURRENT_USER"),QString("HKEY_LOCAL_MACHINE")}) {
                    QSettings registry(root+"\\Software\\Microsoft\\Windows\\CurrentVersion\\App Paths\\"+name,QSettings::NativeFormat);
                    const auto candidate=registry.value(".").toString();
                    if(QFileInfo::exists(candidate)) { executable=candidate; break; }
                }
                if(executable.isEmpty()) executable=QStandardPaths::findExecutable(name);
            }
            if(!QFileInfo::exists(executable)) { error=QCoreApplication::translate("MouseWheel","未找到所选浏览器，可选择自定义浏览器路径。"); return false; }
            return openFile(executable,'"'+QString::fromLatin1(url.toEncoded())+'"',QFileInfo(executable).absolutePath(),true,error);
        }
    }
    if(const auto* a=std::get_if<FolderAction>(&action)) {
        QString path=a->path;
        switch(a->location) {
        case FolderLocation::Path: break;
        case FolderLocation::Desktop: path=QStandardPaths::writableLocation(QStandardPaths::DesktopLocation); break;
        case FolderLocation::Downloads: path=QStandardPaths::writableLocation(QStandardPaths::DownloadLocation); break;
        case FolderLocation::Documents: path=QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation); break;
        case FolderLocation::Pictures: path=QStandardPaths::writableLocation(QStandardPaths::PicturesLocation); break;
        case FolderLocation::Home: path=QDir::homePath(); break;
        case FolderLocation::Computer: path="shell:MyComputerFolder"; break;
        case FolderLocation::RecycleBin: path="shell:RecycleBinFolder"; break;
        }
        if(!path.startsWith("shell:") && !QFileInfo(path).isDir()) { error=QCoreApplication::translate("MouseWheel","文件夹不存在。"); return false; }
        return openFile(path,{},{},true,error);
    }
    if(const auto* a=std::get_if<CommandAction>(&action)) {
        const QString system=qEnvironmentVariable("SystemRoot")+"/System32/";
        QString program; QStringList args;
        switch(a->shell) {
        case Shell::Cmd: program=system+"cmd.exe"; args={"/D","/S",a->hidden?"/C":"/K",a->script}; break;
        case Shell::PowerShell: {
            program=system+"WindowsPowerShell/v1.0/powershell.exe";
            const QByteArray utf16(reinterpret_cast<const char*>(a->script.utf16()),a->script.size()*2);
            args={"-NoLogo","-NoProfile"}; if(!a->hidden) args<<"-NoExit";
            args<<"-EncodedCommand"<<QString::fromLatin1(utf16.toBase64()); break;
        }
        case Shell::Zsh: error=QStringLiteral("Zsh 仅适用于 macOS。");return false;
        case Shell::Wsl: program=system+"wsl.exe"; args={"--exec","sh","-lc",a->script}; break;
        }
        QProcess process; process.setProgram(program); process.setArguments(a->shell==Shell::Cmd?QStringList{}:args); process.setWorkingDirectory(a->directory);
        if(a->shell==Shell::Cmd) process.setNativeArguments(QString("/D /S %1 \"%2\"").arg(a->hidden?"/C":"/K",a->script));
        process.setCreateProcessArgumentsModifier([hidden=a->hidden](QProcess::CreateProcessArguments* args){
            args->flags&=~DETACHED_PROCESS; args->flags|=hidden?CREATE_NO_WINDOW:CREATE_NEW_CONSOLE;
            if(!hidden) { args->startupInfo->dwFlags&=~STARTF_USESTDHANDLES; args->startupInfo->dwFlags|=STARTF_USESHOWWINDOW; args->startupInfo->wShowWindow=SW_SHOWNORMAL; }
        });
        if(process.startDetached()) return true;
        error=process.errorString(); return false;
    }
    error=QCoreApplication::translate("MouseWheel","无法打开该目标。"); return false;
}
}
