#include "core/model.h"
#include <QKeySequence>
#include <algorithm>
#include <cmath>
#include <numbers>
namespace wheel {
Config defaultConfig() {
    Config c;
    c.slots = {{
        {QStringLiteral("复制"), {Qt::Key_C, bit(Modifier::Control)}},
        {QStringLiteral("粘贴"), {Qt::Key_V, bit(Modifier::Control)}},
        {QStringLiteral("撤销"), {Qt::Key_Z, bit(Modifier::Control)}},
        {QStringLiteral("重做"), {Qt::Key_Y, bit(Modifier::Control)}},
        {QStringLiteral("全选"), {Qt::Key_A, bit(Modifier::Control)}},
        {QStringLiteral("保存"), {Qt::Key_S, bit(Modifier::Control)}},
        {QStringLiteral("区域截图"), {}, ActionKind::Screenshot},
        {QStringLiteral("剪切"), {Qt::Key_X, bit(Modifier::Control)}}
    }};
    return c;
}
bool supportedKey(int key) {
    return (key >= Qt::Key_A && key <= Qt::Key_Z) ||
           (key >= Qt::Key_0 && key <= Qt::Key_9) ||
           (key >= Qt::Key_F1 && key <= Qt::Key_F24) ||
           key == Qt::Key_Space || key == Qt::Key_Tab || key == Qt::Key_Return ||
           key == Qt::Key_Backspace || key == Qt::Key_Delete || key == Qt::Key_Insert ||
           key == Qt::Key_Home || key == Qt::Key_End || key == Qt::Key_PageUp ||
           key == Qt::Key_PageDown || (key >= Qt::Key_Left && key <= Qt::Key_Down) ||
           key == Qt::Key_Escape;
}
QString validate(const Config& c) {
    if (c.modifier != Modifier::None && c.modifier != Modifier::Control && c.modifier != Modifier::Alt &&
        c.modifier != Modifier::Shift && c.modifier != Modifier::Meta)
        return QStringLiteral("请选择一个触发修饰键。");
    if (c.button < MouseButton::Right || c.button > MouseButton::Forward ||
        c.theme < Theme::Light || c.theme > Theme::Dark)
        return QStringLiteral("配置包含不支持的选项。");
    if (c.modifier == Modifier::None && c.button != MouseButton::Middle)
        return QStringLiteral("单键触发使用鼠标中键。");
    for (const auto& slot : c.slots) {
        if (slot.name.size() > 12) return QStringLiteral("槽位名称最多 12 个字符。");
        if (slot.kind==ActionKind::Screenshot) {
            if(slot.name.trimmed().isEmpty() || slot.shortcut.key || slot.shortcut.modifiers)
                return QStringLiteral("截图动作需要名称，不能包含快捷键。");
            continue;
        }
        if(slot.kind!=ActionKind::Shortcut) return QStringLiteral("不支持的动作类型。");
        if (!slot.shortcut.key) {
            if (!slot.name.isEmpty() || slot.shortcut.modifiers)
                return QStringLiteral("空槽位不能包含名称或修饰键。");
        } else if (slot.name.trimmed().isEmpty() || !supportedKey(slot.shortcut.key) ||
                   (slot.shortcut.modifiers & ~15u)) {
            return QStringLiteral("请为每个动作填写名称和受支持的快捷键。");
        }
    }
    return {};
}
QString shortcutText(const Shortcut& s) {
    if (!s.key) return {};
    Qt::KeyboardModifiers mods;
    if (s.modifiers & bit(Modifier::Control)) mods |= Qt::ControlModifier;
    if (s.modifiers & bit(Modifier::Alt)) mods |= Qt::AltModifier;
    if (s.modifiers & bit(Modifier::Shift)) mods |= Qt::ShiftModifier;
    if (s.modifiers & bit(Modifier::Meta)) mods |= Qt::MetaModifier;
    return QKeySequence(QKeyCombination(mods, static_cast<Qt::Key>(s.key))).toString(QKeySequence::NativeText);
}
Geometry Geometry::fit(QPointF p, QRectF area, double scale) {
    const double r = std::min({164 * scale, area.width() / 2, area.height() / 2});
    return {{std::clamp(p.x(), area.left()+r, area.right()-r),
             std::clamp(p.y(), area.top()+r, area.bottom()-r)}, r, r * 42 / 164};
}
int Geometry::hit(QPointF p) const {
    const auto delta = p-center;
    const auto distance = std::hypot(delta.x(), delta.y());
    if (distance <= deadRadius || distance > radius) return -1;
    auto angle = std::atan2(delta.x(), -delta.y()) + std::numbers::pi / 8;
    if (angle < 0) angle += 2 * std::numbers::pi;
    return static_cast<int>(angle / (std::numbers::pi / 4)) % 8;
}
}
