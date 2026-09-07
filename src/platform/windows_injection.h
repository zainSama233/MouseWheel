#pragma once
#include "core/model.h"
#include <Windows.h>
#include <array>
#include <vector>
namespace wheel::win {
inline constexpr ULONG_PTR injectionTag = 0x4D57484C;
inline constexpr std::array<WORD, 8> modifierKeys{VK_LCONTROL,VK_RCONTROL,VK_LMENU,VK_RMENU,
                                               VK_LSHIFT,VK_RSHIFT,VK_LWIN,VK_RWIN};
using KeyState = std::array<bool, 256>;
WORD virtualKey(int key);
Modifiers modifiers(const KeyState& state);
INPUT keyEvent(WORD key, bool down);
std::vector<INPUT> injectionPlan(const Shortcut& shortcut, const KeyState& physical);
std::vector<INPUT> recoveryPlan(const std::vector<INPUT>& plan, size_t sent, const KeyState& physical);
bool sendShortcut(const Shortcut& shortcut, const KeyState& physical, QString& error);
}
