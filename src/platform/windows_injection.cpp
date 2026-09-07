#include "platform/windows_injection.h"
#include <algorithm>
namespace wheel::win {
WORD virtualKey(int key) {
    if ((key >= Qt::Key_A && key <= Qt::Key_Z) || (key >= Qt::Key_0 && key <= Qt::Key_9))
        return static_cast<WORD>(key);
    if (key >= Qt::Key_F1 && key <= Qt::Key_F24) return static_cast<WORD>(VK_F1 + key - Qt::Key_F1);
    switch(key) {
    case Qt::Key_Space: return VK_SPACE; case Qt::Key_Tab: return VK_TAB;
    case Qt::Key_Return: return VK_RETURN; case Qt::Key_Backspace: return VK_BACK;
    case Qt::Key_Delete: return VK_DELETE; case Qt::Key_Insert: return VK_INSERT;
    case Qt::Key_Home: return VK_HOME; case Qt::Key_End: return VK_END;
    case Qt::Key_PageUp: return VK_PRIOR; case Qt::Key_PageDown: return VK_NEXT;
    case Qt::Key_Left: return VK_LEFT; case Qt::Key_Right: return VK_RIGHT;
    case Qt::Key_Up: return VK_UP; case Qt::Key_Down: return VK_DOWN;
    case Qt::Key_Escape: return VK_ESCAPE; default: return 0;
    }
}
Modifiers modifiers(const KeyState& s) {
    return ((s[VK_LCONTROL] || s[VK_RCONTROL]) ? bit(Modifier::Control) : 0) |
           ((s[VK_LMENU] || s[VK_RMENU]) ? bit(Modifier::Alt) : 0) |
           ((s[VK_LSHIFT] || s[VK_RSHIFT]) ? bit(Modifier::Shift) : 0) |
           ((s[VK_LWIN] || s[VK_RWIN]) ? bit(Modifier::Meta) : 0);
}
INPUT keyEvent(WORD vk, bool down) {
    INPUT input{}; input.type = INPUT_KEYBOARD; input.ki.wVk = vk;
    input.ki.dwExtraInfo = injectionTag;
    if (!down) input.ki.dwFlags |= KEYEVENTF_KEYUP;
    if (vk == VK_RCONTROL || vk == VK_RMENU || vk == VK_LWIN || vk == VK_RWIN ||
        (vk >= VK_PRIOR && vk <= VK_DOWN) || vk == VK_INSERT || vk == VK_DELETE)
        input.ki.dwFlags |= KEYEVENTF_EXTENDEDKEY;
    return input;
}
std::vector<INPUT> injectionPlan(const Shortcut& action, const KeyState& state) {
    const WORD key = virtualKey(action.key);
    if (!key || state[key]) return {};
    std::vector<INPUT> plan;
    std::vector<WORD> neutralized, added;
    const std::array<Modifier,4> groups{Modifier::Control,Modifier::Alt,Modifier::Shift,Modifier::Meta};
    const bool maskMenu = (modifiers(state) & (bit(Modifier::Alt)|bit(Modifier::Meta))) != 0;
    if (maskMenu) { plan.push_back(keyEvent(0xE8,true)); plan.push_back(keyEvent(0xE8,false)); }
    for (size_t group=0; group<groups.size(); ++group) {
        const WORD left = modifierKeys[group*2], right = modifierKeys[group*2+1];
        if (!(action.modifiers & bit(groups[group]))) {
            for (const WORD vk : {left,right}) if (state[vk]) {
                neutralized.push_back(vk); plan.push_back(keyEvent(vk,false));
            }
        } else if (!state[left] && !state[right]) {
            added.push_back(left); plan.push_back(keyEvent(left,true));
        }
    }
    plan.push_back(keyEvent(key,true)); plan.push_back(keyEvent(key,false));
    for (auto it=added.rbegin(); it!=added.rend(); ++it) plan.push_back(keyEvent(*it,false));
    for (WORD vk : neutralized) plan.push_back(keyEvent(vk,true));
    if (maskMenu) { plan.push_back(keyEvent(0xE8,true)); plan.push_back(keyEvent(0xE8,false)); }
    return plan;
}
std::vector<INPUT> recoveryPlan(const std::vector<INPUT>& plan, size_t sent, const KeyState& physical) {
    KeyState logical = physical;
    KeyState touched{};
    for (size_t i=0; i<std::min(sent,plan.size()); ++i) {
        const auto vk = plan[i].ki.wVk;
        logical[vk] = !(plan[i].ki.dwFlags & KEYEVENTF_KEYUP);
        touched[vk] = true;
    }
    std::vector<INPUT> result;
    for (size_t i=0; i<256; ++i)
        if (touched[i] && logical[i] && !physical[i]) result.push_back(keyEvent(static_cast<WORD>(i),false));
    for (WORD vk : modifierKeys)
        if (touched[vk] && !logical[vk] && physical[vk]) result.push_back(keyEvent(vk,true));
    return result;
}
bool sendShortcut(const Shortcut& shortcut, const KeyState& physical, QString& error) {
    auto plan = injectionPlan(shortcut, physical);
    if (plan.empty()) {
        error = QStringLiteral("动作键仍被按住或不受支持，本次动作已取消。"); return false;
    }
    const UINT sent = SendInput(static_cast<UINT>(plan.size()), plan.data(), sizeof(INPUT));
    if (sent == plan.size()) return true;
    auto cleanup = recoveryPlan(plan, sent, physical);
    const auto cleaned = cleanup.empty() ? 0u : SendInput(static_cast<UINT>(cleanup.size()), cleanup.data(), sizeof(INPUT));
    error = QStringLiteral("快捷键发送失败（%1/%2）。请检查目标应用权限。").arg(sent).arg(plan.size());
    if (cleaned != cleanup.size()) error += QStringLiteral("输入恢复失败，请松开修饰键后重试。");
    return false;
}
}
