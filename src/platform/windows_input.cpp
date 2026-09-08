#include "platform/input_service.h"
#include "platform/windows_injection.h"
#include "platform/desktop_actions.h"
#include "platform/window_context.h"
#include "core/interaction.h"
#include "core/clock.h"
#include <QThread>
#include <QMetaObject>
#include <Windows.h>
#include <shellscalingapi.h>
#include <wtsapi32.h>
namespace wheel {
namespace {
struct Hook {
    Hook() = default;
    Hook(const Hook&) = delete;
    Hook& operator=(const Hook&) = delete;
    HHOOK value = nullptr;
    ~Hook() { reset(); }
    void reset(HHOOK next = nullptr) { if (value) UnhookWindowsHookEx(value); value = next; }
};
class WindowsInput final : public QObject {
public:
    explicit WindowsInput(InputService* service) : service_(service) {}
    ~WindowsInput() override {
        keyboard_.reset(); mouse_.reset();
        if (focus_) UnhookWinEvent(focus_);
        if (location_) UnhookWinEvent(location_);
        if (window_) { WTSUnRegisterSessionNotification(window_); DestroyWindow(window_); }
        self_ = nullptr;
    }
    void start(Config config) {
        config_ = std::move(config); self_ = this;
        SetThreadDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);
        const wchar_t* name = L"MouseWheelInputEvents";
        WNDCLASSW cls{}; cls.lpfnWndProc = windowProc;
        cls.hInstance = GetModuleHandleW(nullptr); cls.lpszClassName = name;
        RegisterClassW(&cls);
        window_ = CreateWindowExW(0,name,L"",WS_POPUP,0,0,0,0,nullptr,nullptr,cls.hInstance,nullptr);
        focus_ = SetWinEventHook(EVENT_SYSTEM_FOREGROUND,EVENT_SYSTEM_FOREGROUND,nullptr,
                                focusProc,0,0,WINEVENT_OUTOFCONTEXT);
        location_=SetWinEventHook(EVENT_OBJECT_LOCATIONCHANGE,EVENT_OBJECT_LOCATIONCHANGE,nullptr,locationProc,0,0,WINEVENT_OUTOFCONTEXT);
        refreshContext();
        notificationsReady_ = window_ && focus_ && location_ &&
                              WTSRegisterSessionNotification(window_, NOTIFY_FOR_THIS_SESSION);
        install();
    }
    void configure(Config config) {
        config_ = std::move(config);
        if(!contextAllows(GetForegroundWindow())) {pending_.reset(); publish(core_.cancel());}
    }
    void pause(bool paused) {
        paused_ = paused;
        pending_.reset();
        publish(core_.setPaused(paused_ || locked_ || sleeping_));
    }
    void restart() {
        pending_.reset(); publish(core_.cancel()); install();
    }
    void hidden(quint64 session) {
        if (!pending_ || pending_->session != session) return;
        const auto pending = *pending_; pending_.reset();
        if (paused_ || locked_ || sleeping_ || core_.session() != session ||
            !IsWindow(reinterpret_cast<HWND>(pending.target)) ||
            GetForegroundWindow() != reinterpret_cast<HWND>(pending.target) ||
            !contextAllows(reinterpret_cast<HWND>(pending.target))) return;
        if (pending.action->kind()!=ActionKind::Shortcut && pending.action->kind()!=ActionKind::Window && pending.action->kind()!=ActionKind::System) { Q_EMIT service_->actionRequested(*pending.action); return; }
        refreshPhysical();
        QString error;
        const auto* shortcut=std::get_if<Shortcut>(&pending.action->action);
        const bool ok=shortcut?win::sendShortcut(*shortcut,physical_,error):win::executeDesktopAction(pending.action->action,reinterpret_cast<HWND>(pending.target),physical_,error);
        if(!ok) Q_EMIT service_->failure(error);
    }
private:
    void refreshContext() {
        foreground_=GetForegroundWindow(); executable_=win::processExecutable(foreground_); fullscreen_=win::isFullscreen(foreground_);
    }
    bool contextAllows(HWND window) const {
        const auto& rules=config_.triggerRules;
        if(!rules.pauseFullscreen && rules.excludedApplications.isEmpty()) return true;
        return window==foreground_ && rules.allows(executable_,fullscreen_);
    }
    void refreshPhysical() {
        for (int vk=0; vk<256; ++vk) physical_[vk] = (GetAsyncKeyState(vk) & 0x8000) != 0;
    }
    void install() {
        keyboard_.reset(); mouse_.reset();
        if (!notificationsReady_) {
            Q_EMIT service_->failure(QStringLiteral("系统状态监听不可用，输入监听已停止。"));
            Q_EMIT service_->listening(false); return;
        }
        refreshPhysical();
        for (auto [button,vk] : {std::pair{MouseButton::Right,VK_RBUTTON},
             {MouseButton::Middle,VK_MBUTTON},{MouseButton::Back,VK_XBUTTON1},
             {MouseButton::Forward,VK_XBUTTON2}})
            if (!physical_[vk]) core_.release(button,{});
        if (!physical_[VK_ESCAPE]) core_.escape(false);
        keyboard_.reset(SetWindowsHookExW(WH_KEYBOARD_LL,keyboardProc,GetModuleHandleW(nullptr),0));
        mouse_.reset(SetWindowsHookExW(WH_MOUSE_LL,mouseProc,GetModuleHandleW(nullptr),0));
        const bool ok = keyboard_.value && mouse_.value;
        if (!ok) {
            keyboard_.reset(); mouse_.reset();
            Q_EMIT service_->failure(QStringLiteral("全局输入监听启动失败（%1）。").arg(GetLastError()));
        }
        Q_EMIT service_->listening(ok);
    }
    void publish(const Decision& d) {
        if (d.show) {
            pending_.reset();
            Q_EMIT service_->showWheel(d.session,core_.snapshot(),core_.geometry(),screen_);
        }
        if (d.selectionChanged) Q_EMIT service_->selection(d.session,d.selection);
        if (d.hide) {
            if (d.action) pending_ = d;
            else pending_.reset();
            Q_EMIT service_->hideWheel(d.session);
        }
    }
    static LRESULT CALLBACK keyboardProc(int code, WPARAM message, LPARAM data) {
        if (code < 0 || !self_) return CallNextHookEx(nullptr,code,message,data);
        const auto& event = *reinterpret_cast<KBDLLHOOKSTRUCT*>(data);
        if (event.dwExtraInfo == win::injectionTag) return CallNextHookEx(nullptr,code,message,data);
        const bool down = message == WM_KEYDOWN || message == WM_SYSKEYDOWN;
        if (event.vkCode < 256)
            self_->physical_[event.vkCode] = down;
        if (event.vkCode == VK_ESCAPE) {
            const auto decision = self_->core_.escape(down);
            self_->publish(decision);
            if (decision.consumed) return 1;
        }
        return CallNextHookEx(nullptr,code,message,data);
    }
    static LRESULT CALLBACK mouseProc(int code, WPARAM message, LPARAM data) {
        if (code < 0 || !self_) return CallNextHookEx(nullptr,code,message,data);
        const auto& event = *reinterpret_cast<MSLLHOOKSTRUCT*>(data);
        if (event.dwExtraInfo == win::injectionTag) return CallNextHookEx(nullptr,code,message,data);
        const QPointF point(event.pt.x,event.pt.y);
        Decision decision;
        if (message == WM_MOUSEMOVE) decision = self_->core_.move(point);
        else {
            std::optional<MouseButton> button;
            bool down = false;
            switch(message) {
            case WM_RBUTTONDOWN: down = true; [[fallthrough]];
            case WM_RBUTTONUP: button = MouseButton::Right; break;
            case WM_MBUTTONDOWN: down = true; [[fallthrough]];
            case WM_MBUTTONUP: button = MouseButton::Middle; break;
            case WM_XBUTTONDOWN: down = true; [[fallthrough]];
            case WM_XBUTTONUP: button = HIWORD(event.mouseData) == XBUTTON1 ? MouseButton::Back : MouseButton::Forward; break;
            default: break;
            }
            if (button) {
                if (down) {
                    const auto onset = monotonicNanos();
                    Geometry geometry;
                    const auto mods = win::modifiers(self_->physical_);
                    if (*button == self_->config_.button && mods == bit(self_->config_.modifier)) {
                        const auto monitor = MonitorFromPoint(event.pt,MONITOR_DEFAULTTONEAREST);
                        MONITORINFOEXW info{}; info.cbSize = sizeof(info);
                        if (!GetMonitorInfoW(monitor,&info)) return CallNextHookEx(nullptr,code,message,data);
                        UINT dx=96, dy=96;
                        GetDpiForMonitor(monitor,MDT_EFFECTIVE_DPI,&dx,&dy);
                        const auto& r = info.rcWork;
                        geometry = Geometry::fit(point,{double(r.left),double(r.top),double(r.right-r.left),double(r.bottom-r.top)},dx/96.0);
                        self_->screen_ = QString::fromWCharArray(info.szDevice);
                    }
                    const HWND target=GetForegroundWindow();
                    decision = self_->core_.press(*button,mods,self_->config_,geometry,
                                                 reinterpret_cast<quintptr>(target),self_->contextAllows(target));
                    if (decision.show) Q_EMIT self_->service_->triggered(decision.session,onset);
                } else decision = self_->core_.release(*button,point);
            }
        }
        self_->publish(decision);
        if (decision.consumed) return 1;
        return CallNextHookEx(nullptr,code,message,data);
    }
    static void CALLBACK focusProc(HWINEVENTHOOK,DWORD,HWND,LONG,LONG,DWORD,DWORD) {
        if (!self_) return;
        self_->pending_.reset();
        self_->publish(self_->core_.cancel()); self_->refreshContext();
    }
    static void CALLBACK locationProc(HWINEVENTHOOK,DWORD,HWND window,LONG object,LONG child,DWORD,DWORD) {
        if(!self_ || object!=OBJID_WINDOW || child!=CHILDID_SELF || window!=self_->foreground_) return;
        self_->fullscreen_=win::isFullscreen(window);
        if(!self_->contextAllows(window)) {self_->pending_.reset(); self_->publish(self_->core_.cancel());}
    }
    static LRESULT CALLBACK windowProc(HWND hwnd,UINT message,WPARAM wp,LPARAM lp) {
        if (self_) {
            const bool lock = message == WM_WTSSESSION_CHANGE && wp == WTS_SESSION_LOCK;
            const bool unlock = message == WM_WTSSESSION_CHANGE && wp == WTS_SESSION_UNLOCK;
            const bool sleep = message == WM_POWERBROADCAST && wp == PBT_APMSUSPEND;
            const bool wake = message == WM_POWERBROADCAST &&
                              (wp == PBT_APMRESUMEAUTOMATIC || wp == PBT_APMRESUMESUSPEND);
            const bool resume = unlock || wake;
            if (lock || unlock || sleep || wake || message == WM_DISPLAYCHANGE) {
                self_->pending_.reset();
                if (lock || unlock) self_->locked_ = lock;
                if (sleep || wake) self_->sleeping_ = sleep;
                self_->publish(self_->core_.cancel());
                self_->publish(self_->core_.setPaused(self_->paused_ || self_->locked_ || self_->sleeping_));
                if (resume || message == WM_DISPLAYCHANGE) {
                    self_->refreshContext(); self_->install();

                }
            }
        }
        return DefWindowProcW(hwnd,message,wp,lp);
    }
    static thread_local WindowsInput* self_;
    InputService* service_;
    Hook keyboard_, mouse_;
    HWINEVENTHOOK focus_ = nullptr;
    HWINEVENTHOOK location_ = nullptr;
    HWND foreground_ = nullptr;
    QString executable_;
    bool fullscreen_ = false;
    HWND window_ = nullptr;
    Interaction core_;
    Config config_;
    QString screen_;
    bool paused_ = false, locked_ = false, sleeping_ = false;
    bool notificationsReady_ = false;
    win::KeyState physical_{};
    std::optional<Decision> pending_;
};
thread_local WindowsInput* WindowsInput::self_ = nullptr;
}
struct InputService::Impl {
    QThread thread;
    WindowsInput* worker;
    explicit Impl(InputService* owner) : worker(new WindowsInput(owner)) {
        worker->moveToThread(&thread);
        QObject::connect(&thread,&QThread::finished,worker,&QObject::deleteLater);
    }
    ~Impl() { thread.quit(); thread.wait(); }
};
InputService::InputService(QObject* parent) : QObject(parent), impl_(std::make_unique<Impl>(this)) {}
InputService::~InputService() = default;
void InputService::start(Config config) {
    impl_->thread.start();
    QMetaObject::invokeMethod(impl_->worker,[w=impl_->worker,config]{ w->start(config); });
}
void InputService::configure(Config config) {
    QMetaObject::invokeMethod(impl_->worker,[w=impl_->worker,config]{ w->configure(config); });
}
void InputService::pause(bool paused) {
    QMetaObject::invokeMethod(impl_->worker,[w=impl_->worker,paused]{ w->pause(paused); });
}
void InputService::hidden(quint64 session) {
    QMetaObject::invokeMethod(impl_->worker,[w=impl_->worker,session]{ w->hidden(session); });
}
void InputService::restart() {
    QMetaObject::invokeMethod(impl_->worker,[w=impl_->worker]{ w->restart(); });
}
}
