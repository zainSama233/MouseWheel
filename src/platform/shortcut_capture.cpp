#include "platform/shortcut_capture.h"
#include "platform/windows_injection.h"
#include <QGuiApplication>
#include <QTimer>
#include <algorithm>
namespace wheel {
struct ShortcutCapture::Impl {
    static ShortcutCapture* active;
    HHOOK hook=nullptr;
    win::KeyState held{},prior{};
    Shortcut captured;
    bool cancelled=false;
    static LRESULT CALLBACK callback(int code,WPARAM message,LPARAM data) {
        if(code<0 || !active) return CallNextHookEx(nullptr,code,message,data);
        auto* owner=active;auto& self=*owner->impl_;const auto& event=*reinterpret_cast<KBDLLHOOKSTRUCT*>(data);
        if(event.vkCode>=256 || event.dwExtraInfo==win::injectionTag) return CallNextHookEx(nullptr,code,message,data);
        const bool down=message==WM_KEYDOWN || message==WM_SYSKEYDOWN;
        if(self.prior[event.vkCode]) {if(!down) self.prior[event.vkCode]=false;return CallNextHookEx(nullptr,code,message,data);}
        if(self.cancelled && !self.held[event.vkCode]) return CallNextHookEx(nullptr,code,message,data);
        self.held[event.vkCode]=down;
        if(down && !self.captured.key && !self.cancelled) {
            int key=0;
            for(int k=Qt::Key_A;k<=Qt::Key_Z;++k) if(win::virtualKey(k)==event.vkCode) key=k;
            for(int k=Qt::Key_0;k<=Qt::Key_9;++k) if(win::virtualKey(k)==event.vkCode) key=k;
            for(int k=Qt::Key_Escape;k<=Qt::Key_F24;++k) if(supportedKey(k) && win::virtualKey(k)==event.vkCode) key=k;
            if(event.vkCode==VK_CANCEL) key=Qt::Key_Cancel;
            if(event.vkCode==VK_SPACE) key=Qt::Key_Space;
            auto state=self.held;for(size_t i=0;i<state.size();++i) state[i]=state[i]||self.prior[i];
            if(key) self.captured={key,win::modifiers(state)};
        }
        if(event.vkCode==VK_PAUSE) self.held[VK_PAUSE]=false;
        if((!down || event.vkCode==VK_PAUSE) && (self.captured.key || self.cancelled)) QTimer::singleShot(0,owner,[owner]{owner->finish();});
        return 1;
    }
};
ShortcutCapture* ShortcutCapture::Impl::active=nullptr;
ShortcutCapture::ShortcutCapture(QObject* owner):QObject(qGuiApp),impl_(std::make_unique<Impl>()) {
    connect(owner,&QObject::destroyed,this,&ShortcutCapture::cancel);
    connect(qGuiApp,&QGuiApplication::applicationStateChanged,this,[this](auto state){if(state!=Qt::ApplicationActive)cancel();});
}
ShortcutCapture::~ShortcutCapture() {if(impl_->hook)UnhookWindowsHookEx(impl_->hook);if(Impl::active==this)Impl::active=nullptr;}
ShortcutCapture* ShortcutCapture::active() {return Impl::active;}
bool ShortcutCapture::start() {
    if(Impl::active) {deleteLater();return false;}
    for(int i=0;i<256;++i) impl_->prior[i]=(GetAsyncKeyState(i)&0x8000)!=0;
    Impl::active=this;impl_->hook=SetWindowsHookExW(WH_KEYBOARD_LL,Impl::callback,GetModuleHandleW(nullptr),0);
    if(!impl_->hook) {Impl::active=nullptr;deleteLater();return false;}return true;
}
void ShortcutCapture::cancel() {impl_->cancelled=true;finish();}
void ShortcutCapture::finish() {
    if(!impl_->hook || std::any_of(impl_->held.begin(),impl_->held.end(),[](bool down){return down;}))return;
    UnhookWindowsHookEx(impl_->hook);impl_->hook=nullptr;Impl::active=nullptr;
    if(impl_->captured.key && !impl_->cancelled)Q_EMIT recorded(impl_->captured);
    Q_EMIT stopped();deleteLater();
}
}
