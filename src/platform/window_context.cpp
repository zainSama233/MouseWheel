#include "platform/window_context.h"
#include <QDir>
#include <QScopeGuard>
#include <dwmapi.h>
namespace wheel::win {
QString processExecutable(HWND window) {
    DWORD pid=0; GetWindowThreadProcessId(window,&pid);
    if(!pid) return {};
    const HANDLE process=OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION,FALSE,pid);
    if(!process) return {};
    const auto cleanup=qScopeGuard([&]{CloseHandle(process);});
    wchar_t buffer[32768]; DWORD length=32768;
    return QueryFullProcessImageNameW(process,0,buffer,&length)?QDir::fromNativeSeparators(QString::fromWCharArray(buffer,int(length))):QString{};
}
bool isFullscreen(HWND window) {
    if(!window || !IsWindowVisible(window) || IsIconic(window) || window==GetShellWindow() || window==GetDesktopWindow()) return false;
    MONITORINFO monitor{}; monitor.cbSize=sizeof(monitor);
    if(!GetMonitorInfoW(MonitorFromWindow(window,MONITOR_DEFAULTTONEAREST),&monitor)) return false;
    RECT bounds{};
    if(FAILED(DwmGetWindowAttribute(window,DWMWA_EXTENDED_FRAME_BOUNDS,&bounds,sizeof(bounds))) && !GetWindowRect(window,&bounds)) return false;
    const auto& screen=monitor.rcMonitor;
    return bounds.left<=screen.left && bounds.top<=screen.top && bounds.right>=screen.right && bounds.bottom>=screen.bottom;
}
}
