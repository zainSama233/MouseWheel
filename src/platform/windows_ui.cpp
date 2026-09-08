#include "platform/native_ui.h"
#include <QWidget>
#include <QScreen>
#include <QCoreApplication>
#include <Windows.h>
#include <dwmapi.h>
namespace wheel::platform {
bool overlayEvent(void* message,qintptr* result){const auto* msg=static_cast<MSG*>(message);if(msg->message==WM_MOUSEACTIVATE){*result=MA_NOACTIVATE;return true;}if(msg->message==WM_NCHITTEST){*result=HTTRANSPARENT;return true;}return false;}
void initializeDisplay(){SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);}
void setOverlayInput(QWidget* widget,bool transparent) {
    const auto hwnd=reinterpret_cast<HWND>(widget->winId());auto flags=GetWindowLongPtrW(hwnd,GWL_EXSTYLE);
    flags=transparent?flags|WS_EX_TRANSPARENT:flags&~WS_EX_TRANSPARENT;
    SetWindowLongPtrW(hwnd,GWL_EXSTYLE,flags|WS_EX_NOACTIVATE|WS_EX_TOOLWINDOW);
    SetWindowPos(hwnd,nullptr,0,0,0,0,SWP_NOMOVE|SWP_NOSIZE|SWP_NOZORDER|SWP_NOACTIVATE|SWP_FRAMECHANGED);
}
void placeOverlay(QWidget* widget,const Geometry& geometry,bool visible) {
    widget->resize(qRound(geometry.radius*2/widget->screen()->devicePixelRatio()),qRound(geometry.radius*2/widget->screen()->devicePixelRatio()));
    if(visible)widget->show();
    SetWindowPos(reinterpret_cast<HWND>(widget->winId()),HWND_TOPMOST,qRound(geometry.center.x()-geometry.radius),qRound(geometry.center.y()-geometry.radius),qRound(geometry.radius*2),qRound(geometry.radius*2),SWP_NOACTIVATE|(visible?SWP_SHOWWINDOW:0));
}
QImage captureBackdrop(QWidget* widget,const Geometry& geometry) {
    POINT point{LONG(geometry.center.x()),LONG(geometry.center.y())};MONITORINFO info{};info.cbSize=sizeof(info);
    if(!GetMonitorInfoW(MonitorFromPoint(point,MONITOR_DEFAULTTONEAREST),&info))return {};
    auto image=widget->screen()->grabWindow(0).toImage().copy(QRect(qRound(geometry.center.x()-geometry.radius-info.rcMonitor.left),qRound(geometry.center.y()-geometry.radius-info.rcMonitor.top),qRound(geometry.radius*2),qRound(geometry.radius*2)));
    image.setDevicePixelRatio(1);return image;
}
bool prepareCapture(QString& error){if(SUCCEEDED(DwmFlush()))return true;error=QCoreApplication::translate("MouseWheel","无法同步桌面画面，请重试。");return false;}
QString defaultConfigPath(){return QCoreApplication::applicationDirPath()+"/config.json";}
}
