#include "platform/desktop_actions.h"
#include <vector>
#include <algorithm>
namespace wheel::win {
bool executeDesktopAction(const Action& action,HWND target,const KeyState& physical,QString& error) {
    if(const auto* a=std::get_if<SystemAction>(&action)) {
        Shortcut key;
        switch(a->operation) {
        case SystemOperation::Lock: if(LockWorkStation()) return true; error=QStringLiteral("锁屏失败。"); return false;
        case SystemOperation::VolumeUp: key.key=Qt::Key_VolumeUp; break;
        case SystemOperation::VolumeDown: key.key=Qt::Key_VolumeDown; break;
        case SystemOperation::Mute: key.key=Qt::Key_VolumeMute; break;
        case SystemOperation::PlayPause: key.key=Qt::Key_MediaTogglePlayPause; break;
        case SystemOperation::NextTrack: key.key=Qt::Key_MediaNext; break;
        case SystemOperation::PreviousTrack: key.key=Qt::Key_MediaPrevious; break;
        case SystemOperation::TaskView: key={Qt::Key_Tab,bit(Modifier::Meta)}; break;
        case SystemOperation::DesktopLeft: key={Qt::Key_Left,bit(Modifier::Meta)|bit(Modifier::Control)}; break;
        case SystemOperation::DesktopRight: key={Qt::Key_Right,bit(Modifier::Meta)|bit(Modifier::Control)}; break;
        case SystemOperation::NewDesktop: key={Qt::Key_D,bit(Modifier::Meta)|bit(Modifier::Control)}; break;
        case SystemOperation::CloseDesktop: key={Qt::Key_F4,bit(Modifier::Meta)|bit(Modifier::Control)}; break;
        case SystemOperation::ShowDesktop: key={Qt::Key_D,bit(Modifier::Meta)}; break;
        }
        return sendShortcut(key,physical,error);
    }
    const auto* a=std::get_if<WindowAction>(&action);
    if(!a || !IsWindow(target)) { error=QStringLiteral("目标窗口已关闭。"); return false; }
    if(a->operation==WindowOperation::Switch) return sendShortcut({Qt::Key_Tab,bit(Modifier::Alt)},physical,error);
    bool ok=true;
    switch(a->operation) {
    case WindowOperation::TileLeft: case WindowOperation::TileRight: {
        MONITORINFO info{sizeof(info)};
        if(!GetMonitorInfoW(MonitorFromWindow(target,MONITOR_DEFAULTTONEAREST),&info)) { ok=false; break; }
        const auto r=info.rcWork; const int middle=(r.left+r.right)/2;
        const int left=a->operation==WindowOperation::TileLeft?r.left:middle;
        const int right=a->operation==WindowOperation::TileLeft?middle:r.right;
        ShowWindow(target,SW_RESTORE); ok=SetWindowPos(target,nullptr,left,r.top,right-left,r.bottom-r.top,SWP_NOZORDER|SWP_NOACTIVATE); break;
    }
    case WindowOperation::NextMonitor: {
        std::vector<HMONITOR> monitors;
        EnumDisplayMonitors(nullptr,nullptr,[](HMONITOR m,HDC,LPRECT,LPARAM data)->BOOL { reinterpret_cast<std::vector<HMONITOR>*>(data)->push_back(m); return TRUE; },reinterpret_cast<LPARAM>(&monitors));
        if(monitors.size()<2) { error=QStringLiteral("当前只有一个显示器。"); return false; }
        auto current=MonitorFromWindow(target,MONITOR_DEFAULTTONEAREST);
        const auto it=std::find(monitors.begin(),monitors.end(),current);
        const auto next=monitors[(std::distance(monitors.begin(),it)+1)%monitors.size()];
        MONITORINFO from{sizeof(from)},to{sizeof(to)}; RECT rect{};
        if(!GetMonitorInfoW(current,&from) || !GetMonitorInfoW(next,&to) || !GetWindowRect(target,&rect)) { ok=false; break; }
        const bool maximized=IsZoomed(target); if(maximized) ShowWindow(target,SW_RESTORE);
        const int width=std::min(rect.right-rect.left,to.rcWork.right-to.rcWork.left),height=std::min(rect.bottom-rect.top,to.rcWork.bottom-to.rcWork.top);
        const int x=std::clamp(to.rcWork.left+rect.left-from.rcWork.left,to.rcWork.left,to.rcWork.right-width);
        const int y=std::clamp(to.rcWork.top+rect.top-from.rcWork.top,to.rcWork.top,to.rcWork.bottom-height);
        ok=SetWindowPos(target,nullptr,x,y,width,height,SWP_NOZORDER|SWP_NOACTIVATE); if(maximized) ShowWindow(target,SW_MAXIMIZE); break;
    }
    case WindowOperation::Topmost:
        ok=SetWindowPos(target,(GetWindowLongPtrW(target,GWL_EXSTYLE)&WS_EX_TOPMOST)?HWND_NOTOPMOST:HWND_TOPMOST,0,0,0,0,SWP_NOMOVE|SWP_NOSIZE|SWP_NOACTIVATE); break;
    case WindowOperation::Opacity: {
        SetLastError(0); const auto style=GetWindowLongPtrW(target,GWL_EXSTYLE);
        if(!SetWindowLongPtrW(target,GWL_EXSTYLE,style|WS_EX_LAYERED) && GetLastError()) { ok=false; break; }
        ok=SetLayeredWindowAttributes(target,0,BYTE(a->opacity*255/100),LWA_ALPHA); break;
    }
    case WindowOperation::Maximize: ShowWindow(target,IsZoomed(target)?SW_RESTORE:SW_MAXIMIZE); break;
    case WindowOperation::Minimize: ShowWindow(target,SW_MINIMIZE); break;
    case WindowOperation::Switch: break;
    }
    if(!ok) error=QStringLiteral("窗口操作失败（%1）。").arg(GetLastError()); return ok;
}
}
