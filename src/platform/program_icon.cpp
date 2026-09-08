#include "platform/program_icon.h"
#include <QDir>
#include <QScopeGuard>
#include <Windows.h>
#include <shlobj.h>
#include <shellapi.h>
#include <wrl/client.h>
namespace wheel::win {
ShortcutInfo readShortcut(const QString& path) {
    const HRESULT hr=CoInitializeEx(nullptr,COINIT_APARTMENTTHREADED);
    const auto cleanup=qScopeGuard([&]{if(SUCCEEDED(hr)) CoUninitialize();});
    Microsoft::WRL::ComPtr<IShellLinkW> link; Microsoft::WRL::ComPtr<IPersistFile> file;
    if(FAILED(CoCreateInstance(CLSID_ShellLink,nullptr,CLSCTX_INPROC_SERVER,IID_PPV_ARGS(&link))) || FAILED(link.As(&file)) ||
       FAILED(file->Load(reinterpret_cast<LPCWSTR>(path.utf16()),STGM_READ))) return {};
    ShortcutInfo result;
    wchar_t value[32768]{},expanded[32768]{};
    if(SUCCEEDED(link->GetPath(value,32768,nullptr,SLGP_RAWPATH))) {
        const auto length=ExpandEnvironmentStringsW(value,expanded,32768);
        if(length && length<=32768) result.target=QDir::fromNativeSeparators(QString::fromWCharArray(expanded));
    }
    if(SUCCEEDED(link->GetIconLocation(value,32768,&result.iconIndex))) {
        const auto length=ExpandEnvironmentStringsW(value,expanded,32768);
        if(length && length<=32768) result.iconFile=QDir::fromNativeSeparators(QString::fromWCharArray(expanded));
    }
    return result;
}
QImage programIcon(const QString& path) {
    QString source=path; int index=0;
    if(path.endsWith(".lnk",Qt::CaseInsensitive)) {
        const auto shortcut=readShortcut(path);
        if(!shortcut.target.isEmpty()) source=shortcut.target;
        else {source=shortcut.iconFile; index=shortcut.iconIndex;}
    }
    if(source.isEmpty()) return {};
    HICON icon=nullptr;
    SHDefExtractIconW(reinterpret_cast<LPCWSTR>(source.utf16()),index,0,&icon,nullptr,96);
    if(!icon) {
        SHFILEINFOW info{};
        SHGetFileInfoW(reinterpret_cast<LPCWSTR>(source.utf16()),0,&info,sizeof(info),SHGFI_ICON|SHGFI_LARGEICON);
        icon=info.hIcon;
    }
    if(!icon) return {};
    const auto cleanup=qScopeGuard([&]{DestroyIcon(icon);});
    return QImage::fromHICON(icon);
}
}
