#include "platform/macos_actions.h"
#include "platform/macos_keys.h"
#include <QGuiApplication>
#include <QScreen>
#import <AppKit/AppKit.h>
#import <IOKit/hidsystem/ev_keymap.h>
namespace wheel::mac {
struct FocusedWindow {
    AXUIElementRef app=nullptr,window=nullptr;
    explicit FocusedWindow(int pid){app=AXUIElementCreateApplication(pid);if(app)AXUIElementCopyAttributeValue(app,kAXFocusedWindowAttribute,reinterpret_cast<CFTypeRef*>(&window));}
    ~FocusedWindow(){if(window)CFRelease(window);if(app)CFRelease(app);}
};
bool fullscreen(int pid){FocusedWindow target(pid);CFTypeRef value=nullptr;if(!target.window || AXUIElementCopyAttributeValue(target.window,CFSTR("AXFullScreen"),&value)!=kAXErrorSuccess)return false;const bool full=CFEqual(value,kCFBooleanTrue);CFRelease(value);return full;}
bool execute(const Action& action,int pid,QString& error) {
    if(const auto* key=std::get_if<Shortcut>(&action))return sendShortcut(*key,error);
    if(const auto* system=std::get_if<SystemAction>(&action)) {
        const auto control=bit(Modifier::Control),command=bit(Modifier::Meta);int media=-1;
        switch(system->operation) {
        case SystemOperation::Lock:return sendShortcut({Qt::Key_Q,control|command},error);
        case SystemOperation::TaskView:return sendShortcut({Qt::Key_Up,control},error);
        case SystemOperation::DesktopLeft:return sendShortcut({Qt::Key_Left,control},error);
        case SystemOperation::DesktopRight:return sendShortcut({Qt::Key_Right,control},error);
        case SystemOperation::ShowDesktop:return sendShortcut({Qt::Key_F11,0},error);
        case SystemOperation::VolumeUp:media=NX_KEYTYPE_SOUND_UP;break;
        case SystemOperation::VolumeDown:media=NX_KEYTYPE_SOUND_DOWN;break;
        case SystemOperation::Mute:media=NX_KEYTYPE_MUTE;break;
        case SystemOperation::PlayPause:media=NX_KEYTYPE_PLAY;break;
        case SystemOperation::NextTrack:media=NX_KEYTYPE_NEXT;break;
        case SystemOperation::PreviousTrack:media=NX_KEYTYPE_PREVIOUS;break;
        default:error=QStringLiteral("macOS 不提供此虚拟桌面操作，请使用 Mission Control。");return false;
        }
        for(int state:{0xa,0xb}) {NSEvent* event=[NSEvent otherEventWithType:NSEventTypeSystemDefined location:NSZeroPoint modifierFlags:0 timestamp:0 windowNumber:0 context:nil subtype:8 data1:(media<<16)|(state<<8) data2:-1];CGEventRef cg=event.CGEvent;CGEventSetIntegerValueField(cg,kCGEventSourceUserData,InjectionTag);CGEventPost(kCGHIDEventTap,cg);}return true;
    }
    const auto* window=std::get_if<WindowAction>(&action);if(!window)return false;
    if(window->operation==WindowOperation::Switch)return sendShortcut({Qt::Key_Tab,bit(Modifier::Meta)},error);
    if(window->operation==WindowOperation::Topmost || window->operation==WindowOperation::Opacity){error=QStringLiteral("macOS 不允许通过公共接口修改其他应用的置顶或透明度。");return false;}
    FocusedWindow target(pid);if(!target.window){error=QStringLiteral("无法读取前台窗口，请检查辅助功能权限。");return false;}
    if(window->operation==WindowOperation::Minimize)return AXUIElementSetAttributeValue(target.window,kAXMinimizedAttribute,kCFBooleanTrue)==kAXErrorSuccess;
    CGPoint position{};CGSize size{};CFTypeRef point=nullptr,extent=nullptr;
    if(AXUIElementCopyAttributeValue(target.window,kAXPositionAttribute,&point)!=kAXErrorSuccess || AXUIElementCopyAttributeValue(target.window,kAXSizeAttribute,&extent)!=kAXErrorSuccess){if(point)CFRelease(point);if(extent)CFRelease(extent);error=QStringLiteral("无法读取窗口位置。");return false;}
    AXValueGetValue(static_cast<AXValueRef>(point),kAXValueTypeCGPoint,&position);AXValueGetValue(static_cast<AXValueRef>(extent),kAXValueTypeCGSize,&size);CFRelease(point);CFRelease(extent);
    const auto screens=QGuiApplication::screens();int index=0;for(int i=0;i<screens.size();++i)if(screens[i]->geometry().contains(QPoint(position.x+size.width/2,position.y+size.height/2))){index=i;break;}
    if(window->operation==WindowOperation::NextMonitor)index=(index+1)%screens.size();const auto bounds=screens[index]->availableGeometry();
    if(window->operation==WindowOperation::NextMonitor){position={double(bounds.x()),double(bounds.y())};size.width=qMin(size.width,double(bounds.width()));size.height=qMin(size.height,double(bounds.height()));}
    else {position={double(bounds.x()),double(bounds.y())};size={double(bounds.width()),double(bounds.height())};if(window->operation==WindowOperation::TileLeft || window->operation==WindowOperation::TileRight){size.width/=2;if(window->operation==WindowOperation::TileRight)position.x+=size.width;}}
    auto newPoint=AXValueCreate(kAXValueTypeCGPoint,&position);auto newSize=AXValueCreate(kAXValueTypeCGSize,&size);
    const auto moved=AXUIElementSetAttributeValue(target.window,kAXPositionAttribute,newPoint);const auto resized=AXUIElementSetAttributeValue(target.window,kAXSizeAttribute,newSize);CFRelease(newPoint);CFRelease(newSize);
    if(moved==kAXErrorSuccess && resized==kAXErrorSuccess)return true;error=QStringLiteral("目标应用不允许调整窗口。");return false;
}
}
