#include "platform/shortcut_capture.h"
#include "platform/macos_keys.h"
#include <QGuiApplication>
#include <QTimer>
#include <array>
#include <algorithm>
namespace wheel {
struct ShortcutCapture::Impl {
    static ShortcutCapture* active;
    CFMachPortRef tap=nullptr;CFRunLoopSourceRef source=nullptr;
    std::array<bool,128> held{},prior{};Shortcut captured;bool cancelled=false;
    ~Impl(){if(source){CFRunLoopRemoveSource(CFRunLoopGetMain(),source,kCFRunLoopCommonModes);CFRelease(source);}if(tap){CFMachPortInvalidate(tap);CFRelease(tap);}}
    static CGEventRef callback(CGEventTapProxy,CGEventType type,CGEventRef event,void* data) {
        auto* owner=static_cast<ShortcutCapture*>(data);auto& self=*owner->impl_;
        if(type==kCGEventTapDisabledByTimeout || type==kCGEventTapDisabledByUserInput){CGEventTapEnable(self.tap,true);return event;}
        if(CGEventGetIntegerValueField(event,kCGEventSourceUserData)==mac::InjectionTag)return event;
        const auto key=CGEventGetIntegerValueField(event,kCGKeyboardEventKeycode);if(key<0 || key>=128)return event;
        const bool down=type==kCGEventKeyDown || (type==kCGEventFlagsChanged && CGEventSourceKeyState(kCGEventSourceStateHIDSystem,CGKeyCode(key)));
        if(self.prior[key]){if(!down)self.prior[key]=false;return event;}
        if(self.cancelled && !self.held[key])return event;
        self.held[key]=down;
        if(down && type==kCGEventKeyDown && !self.captured.key && !self.cancelled)for(const auto& mapping:mac::Keys)if(mapping.second==key){self.captured={mapping.first,mac::modifiers(CGEventGetFlags(event))};break;}
        if(!down)QTimer::singleShot(0,owner,[owner]{owner->finish();});return nullptr;
    }
};
ShortcutCapture* ShortcutCapture::Impl::active=nullptr;
ShortcutCapture::ShortcutCapture(QObject* owner):QObject(qGuiApp),impl_(std::make_unique<Impl>()) {
    connect(owner,&QObject::destroyed,this,&ShortcutCapture::cancel);
    connect(qGuiApp,&QGuiApplication::applicationStateChanged,this,[this](auto state){if(state!=Qt::ApplicationActive)cancel();});
}
ShortcutCapture::~ShortcutCapture(){if(Impl::active==this)Impl::active=nullptr;}
ShortcutCapture* ShortcutCapture::active(){return Impl::active;}
bool ShortcutCapture::start() {
    if(Impl::active){deleteLater();return false;}
    for(int i=0;i<128;++i)impl_->prior[i]=CGEventSourceKeyState(kCGEventSourceStateHIDSystem,i);
    impl_->tap=CGEventTapCreate(kCGSessionEventTap,kCGHeadInsertEventTap,kCGEventTapOptionDefault,CGEventMaskBit(kCGEventKeyDown)|CGEventMaskBit(kCGEventKeyUp)|CGEventMaskBit(kCGEventFlagsChanged),Impl::callback,this);
    if(!impl_->tap){deleteLater();return false;}impl_->source=CFMachPortCreateRunLoopSource(nullptr,impl_->tap,0);
    CFRunLoopAddSource(CFRunLoopGetMain(),impl_->source,kCFRunLoopCommonModes);CGEventTapEnable(impl_->tap,true);Impl::active=this;return true;
}
void ShortcutCapture::cancel(){impl_->cancelled=true;finish();}
void ShortcutCapture::finish() {
    if(!impl_->tap || std::any_of(impl_->held.begin(),impl_->held.end(),[](bool down){return down;}) || (!impl_->cancelled && !impl_->captured.key))return;
    CGEventTapEnable(impl_->tap,false);Impl::active=nullptr;if(!impl_->cancelled)Q_EMIT recorded(impl_->captured);Q_EMIT stopped();deleteLater();
}
}
