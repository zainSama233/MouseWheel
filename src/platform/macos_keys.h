#pragma once
#include "core/model.h"
#import <Carbon/Carbon.h>
#import <ApplicationServices/ApplicationServices.h>
#include <optional>
namespace wheel::mac {
inline constexpr int64_t InjectionTag=0x4d574d43;
inline constexpr std::pair<int,CGKeyCode> Keys[]={
{Qt::Key_A,kVK_ANSI_A},{Qt::Key_B,kVK_ANSI_B},{Qt::Key_C,kVK_ANSI_C},{Qt::Key_D,kVK_ANSI_D},{Qt::Key_E,kVK_ANSI_E},{Qt::Key_F,kVK_ANSI_F},{Qt::Key_G,kVK_ANSI_G},{Qt::Key_H,kVK_ANSI_H},{Qt::Key_I,kVK_ANSI_I},{Qt::Key_J,kVK_ANSI_J},{Qt::Key_K,kVK_ANSI_K},{Qt::Key_L,kVK_ANSI_L},{Qt::Key_M,kVK_ANSI_M},{Qt::Key_N,kVK_ANSI_N},{Qt::Key_O,kVK_ANSI_O},{Qt::Key_P,kVK_ANSI_P},{Qt::Key_Q,kVK_ANSI_Q},{Qt::Key_R,kVK_ANSI_R},{Qt::Key_S,kVK_ANSI_S},{Qt::Key_T,kVK_ANSI_T},{Qt::Key_U,kVK_ANSI_U},{Qt::Key_V,kVK_ANSI_V},{Qt::Key_W,kVK_ANSI_W},{Qt::Key_X,kVK_ANSI_X},{Qt::Key_Y,kVK_ANSI_Y},{Qt::Key_Z,kVK_ANSI_Z},
{Qt::Key_0,kVK_ANSI_0},{Qt::Key_1,kVK_ANSI_1},{Qt::Key_2,kVK_ANSI_2},{Qt::Key_3,kVK_ANSI_3},{Qt::Key_4,kVK_ANSI_4},{Qt::Key_5,kVK_ANSI_5},{Qt::Key_6,kVK_ANSI_6},{Qt::Key_7,kVK_ANSI_7},{Qt::Key_8,kVK_ANSI_8},{Qt::Key_9,kVK_ANSI_9},
{Qt::Key_F1,kVK_F1},{Qt::Key_F2,kVK_F2},{Qt::Key_F3,kVK_F3},{Qt::Key_F4,kVK_F4},{Qt::Key_F5,kVK_F5},{Qt::Key_F6,kVK_F6},{Qt::Key_F7,kVK_F7},{Qt::Key_F8,kVK_F8},{Qt::Key_F9,kVK_F9},{Qt::Key_F10,kVK_F10},{Qt::Key_F11,kVK_F11},{Qt::Key_F12,kVK_F12},{Qt::Key_F13,kVK_F13},{Qt::Key_F14,kVK_F14},{Qt::Key_F15,kVK_F15},{Qt::Key_F16,kVK_F16},{Qt::Key_F17,kVK_F17},{Qt::Key_F18,kVK_F18},{Qt::Key_F19,kVK_F19},{Qt::Key_F20,kVK_F20},
{Qt::Key_Return,kVK_Return},{Qt::Key_Enter,kVK_ANSI_KeypadEnter},{Qt::Key_Space,kVK_Space},{Qt::Key_Tab,kVK_Tab},{Qt::Key_Escape,kVK_Escape},{Qt::Key_Backspace,kVK_Delete},{Qt::Key_Delete,kVK_ForwardDelete},{Qt::Key_Home,kVK_Home},{Qt::Key_End,kVK_End},{Qt::Key_PageUp,kVK_PageUp},{Qt::Key_PageDown,kVK_PageDown},{Qt::Key_Left,kVK_LeftArrow},{Qt::Key_Right,kVK_RightArrow},{Qt::Key_Up,kVK_UpArrow},{Qt::Key_Down,kVK_DownArrow}};
inline Modifiers modifiers(CGEventFlags flags){return ((flags&kCGEventFlagMaskControl)?bit(Modifier::Control):0)|((flags&kCGEventFlagMaskAlternate)?bit(Modifier::Alt):0)|((flags&kCGEventFlagMaskShift)?bit(Modifier::Shift):0)|((flags&kCGEventFlagMaskCommand)?bit(Modifier::Meta):0);}
inline CGEventFlags flags(Modifiers mods){return ((mods&bit(Modifier::Control))?kCGEventFlagMaskControl:0)|((mods&bit(Modifier::Alt))?kCGEventFlagMaskAlternate:0)|((mods&bit(Modifier::Shift))?kCGEventFlagMaskShift:0)|((mods&bit(Modifier::Meta))?kCGEventFlagMaskCommand:0);}
inline bool sendShortcut(const Shortcut& shortcut,QString& error) {
    std::optional<CGKeyCode> code;for(const auto& key:Keys)if(key.first==shortcut.key){code=key.second;break;}
    if(!code){error=QStringLiteral("此按键不受 macOS 支持。");return false;}
    for(bool down:{true,false}){auto event=CGEventCreateKeyboardEvent(nullptr,*code,down);if(!event){error=QStringLiteral("无法发送快捷键。");return false;}CGEventSetFlags(event,flags(shortcut.modifiers));CGEventSetIntegerValueField(event,kCGEventSourceUserData,InjectionTag);CGEventPost(kCGHIDEventTap,event);CFRelease(event);}return true;
}
}
