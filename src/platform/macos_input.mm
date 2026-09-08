#include "platform/input_service.h"
#include "platform/shortcut_capture.h"
#include "platform/macos_keys.h"
#include "platform/macos_actions.h"
#include "core/interaction.h"
#include "core/screen_helper.h"
#include "core/clock.h"
#include <QGuiApplication>
#include <QScreen>
#include <QTimer>
#import <AppKit/AppKit.h>
namespace wheel {
struct InputService::Impl {
    InputService* owner;Config config;Interaction core;QTimer hover;
    CFMachPortRef tap=nullptr;CFRunLoopSourceRef source=nullptr;
    id focusObserver=nil,sleepObserver=nil,wakeObserver=nil;
    bool paused=false,sleeping=false;std::optional<Decision> pending;QString screen;
    explicit Impl(InputService* service):owner(service){hover.setSingleShot(true);QObject::connect(&hover,&QTimer::timeout,owner,[this]{publish(core.advance(monotonicNanos()/1000000));});}
    ~Impl(){auto* center=NSWorkspace.sharedWorkspace.notificationCenter;for(id observer in @[focusObserver?:[NSNull null],sleepObserver?:[NSNull null],wakeObserver?:[NSNull null]])if(observer!=[NSNull null])[center removeObserver:observer];stop();}
    void stop(){if(source){CFRunLoopRemoveSource(CFRunLoopGetMain(),source,kCFRunLoopCommonModes);CFRelease(source);source=nullptr;}if(tap){CFMachPortInvalidate(tap);CFRelease(tap);tap=nullptr;}}
    void start() {
        stop();pending.reset();publish(core.cancel());
        NSDictionary* options=@{(__bridge NSString*)kAXTrustedCheckOptionPrompt:@YES};
        if(!AXIsProcessTrustedWithOptions((__bridge CFDictionaryRef)options)){Q_EMIT owner->failure(QStringLiteral("请在系统设置 → 隐私与安全性 → 辅助功能中允许 MouseWheel，再点击托盘的重新连接输入。"));Q_EMIT owner->listening(false);return;}
        const CGEventMask mask=CGEventMaskBit(kCGEventMouseMoved)|CGEventMaskBit(kCGEventOtherMouseDragged)|CGEventMaskBit(kCGEventRightMouseDragged)|CGEventMaskBit(kCGEventRightMouseDown)|CGEventMaskBit(kCGEventRightMouseUp)|CGEventMaskBit(kCGEventOtherMouseDown)|CGEventMaskBit(kCGEventOtherMouseUp)|CGEventMaskBit(kCGEventKeyDown)|CGEventMaskBit(kCGEventKeyUp);
        tap=CGEventTapCreate(kCGSessionEventTap,kCGHeadInsertEventTap,kCGEventTapOptionDefault,mask,callback,this);
        if(!tap){Q_EMIT owner->failure(QStringLiteral("无法监听鼠标，请检查辅助功能和输入监控权限。"));Q_EMIT owner->listening(false);return;}
        source=CFMachPortCreateRunLoopSource(nullptr,tap,0);CFRunLoopAddSource(CFRunLoopGetMain(),source,kCFRunLoopCommonModes);CGEventTapEnable(tap,true);Q_EMIT owner->listening(true);
        if(!focusObserver){auto* center=NSWorkspace.sharedWorkspace.notificationCenter;
            focusObserver=[center addObserverForName:NSWorkspaceDidActivateApplicationNotification object:nil queue:NSOperationQueue.mainQueue usingBlock:^(NSNotification*){pending.reset();publish(core.cancel());}];
            sleepObserver=[center addObserverForName:NSWorkspaceWillSleepNotification object:nil queue:NSOperationQueue.mainQueue usingBlock:^(NSNotification*){sleeping=true;pending.reset();publish(core.setPaused(true));}];
            wakeObserver=[center addObserverForName:NSWorkspaceDidWakeNotification object:nil queue:NSOperationQueue.mainQueue usingBlock:^(NSNotification*){sleeping=false;publish(core.setPaused(paused));}];}
    }
    void publish(const Decision& d) {
        const auto deadline=core.hoverDeadline();if(deadline>=0)hover.start(int(qMax<qint64>(1,deadline-monotonicNanos()/1000000)));else hover.stop();
        if(d.show)Q_EMIT owner->showWheel(d.session,core.snapshot(),core.geometry(),screen);
        if(d.levelChanged)Q_EMIT owner->levelChanged(d.session,core.groupIndex());
        if(d.selectionChanged)Q_EMIT owner->selection(d.session,d.selection);
        if(d.hide){if(d.action)pending=d;else pending.reset();Q_EMIT owner->hideWheel(d.session);}
    }
    static CGEventRef callback(CGEventTapProxy,CGEventType type,CGEventRef event,void* data) {
        auto& self=*static_cast<Impl*>(data);
        if(type==kCGEventTapDisabledByTimeout || type==kCGEventTapDisabledByUserInput){self.pending.reset();self.publish(self.core.cancel());CGEventTapEnable(self.tap,true);return event;}
        if(CGEventGetIntegerValueField(event,kCGEventSourceUserData)==mac::InjectionTag || ShortcutCapture::active())return event;
        Decision decision;
        if(type==kCGEventKeyDown || type==kCGEventKeyUp){if(CGEventGetIntegerValueField(event,kCGKeyboardEventKeycode)!=kVK_Escape)return event;decision=self.core.escape(type==kCGEventKeyDown);}
        else {
            const auto location=CGEventGetLocation(event);QPointF point(location.x,location.y);
            if(type==kCGEventMouseMoved || type==kCGEventOtherMouseDragged || type==kCGEventRightMouseDragged)decision=self.core.move(point,monotonicNanos()/1000000);
            else {
                std::optional<MouseButton> button;const auto number=CGEventGetIntegerValueField(event,kCGMouseEventButtonNumber);
                if(number==1)button=MouseButton::Right;else if(number==2)button=MouseButton::Middle;else if(number==3)button=MouseButton::Back;else if(number==4)button=MouseButton::Forward;
                if(!button)return event;const bool down=type==kCGEventRightMouseDown || type==kCGEventOtherMouseDown;
                if(down){NSRunningApplication* app=NSWorkspace.sharedWorkspace.frontmostApplication;
                    const QString path=QString::fromNSString(app.bundleURL.path);const auto resolved=self.config.resolved(path);Geometry geometry;
                    const auto mods=mac::modifiers(CGEventGetFlags(event));
                    if(*button!=self.config.button || mods!=bit(self.config.modifier))return event;
                    auto* display=QGuiApplication::screenAt(point.toPoint());if(!display)return event;
                    geometry=ScreenHelper::fit(point,display->availableGeometry(),1.0,ScreenHelper::extent(resolved),resolved.safetyMargin,resolved.edgePolicy);self.screen=display->name();
                    const auto onset=monotonicNanos();decision=self.core.press(*button,mods,resolved,geometry,app.processIdentifier,self.config.triggerRules.allows(path,mac::fullscreen(app.processIdentifier)));
                    if(decision.show)Q_EMIT self.owner->triggered(decision.session,onset);
                }else decision=self.core.release(*button,point);
            }
        }
        self.publish(decision);return decision.consumed?nullptr:event;
    }
};
InputService::InputService(QObject* parent):QObject(parent),impl_(std::make_unique<Impl>(this)){}
InputService::~InputService()=default;
void InputService::start(Config config){impl_->config=std::move(config);impl_->start();}
void InputService::configure(Config config){impl_->config=std::move(config);}
void InputService::pause(bool paused){impl_->paused=paused;impl_->pending.reset();impl_->publish(impl_->core.setPaused(paused || impl_->sleeping));}
void InputService::restart(){impl_->start();}
void InputService::hidden(quint64 session){
    if(!impl_->pending || impl_->pending->session!=session)return;const auto decision=*impl_->pending;impl_->pending.reset();
    QTimer::singleShot(0,this,[this,decision]{if(impl_->paused || impl_->sleeping || NSWorkspace.sharedWorkspace.frontmostApplication.processIdentifier!=int(decision.target))return;
        const auto app=NSWorkspace.sharedWorkspace.frontmostApplication;
        if(!impl_->config.triggerRules.allows(QString::fromNSString(app.bundleURL.path),mac::fullscreen(app.processIdentifier)))return;
        QString error;const auto& action=decision.action->action;
        if(std::holds_alternative<Shortcut>(action) || std::holds_alternative<WindowAction>(action) || std::holds_alternative<SystemAction>(action)){if(!mac::execute(action,int(decision.target),error))Q_EMIT failure(error);}
        else Q_EMIT actionRequested(*decision.action);
    });
}
}
