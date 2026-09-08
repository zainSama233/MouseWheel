#include "core/interaction.h"
namespace wheel {
Decision Interaction::press(MouseButton b, Modifiers mods, const Config& c,
                            const Geometry& geometry, quintptr target) {
    if (held_ == b) return {.consumed = true};
    if (paused_ || held_ || b != c.button || mods != bit(c.modifier) || !target) return {};
    held_ = b; active_ = true; snapshot_ = c; geometry_ = geometry;
    target_ = target; selection_ = -1; ++session_;
    return {.consumed = true, .show = true, .session = session_};
}
Decision Interaction::release(MouseButton b, QPointF position) {
    if (held_ != b) return {};
    held_.reset();
    Decision result{.consumed = true, .session = session_, .target = target_};
    if (!active_) return result;
    result.hide = true;
    const int selected = geometry_.hit(position,snapshot_.shape);
    if (selected >= 0 && snapshot_.slots[selected].enabled())
        result.action = snapshot_.slots[selected];
    active_ = false;
    return result;
}
Decision Interaction::move(QPointF position) {
    if (!active_) return {};
    const auto selected = geometry_.hit(position,snapshot_.shape);
    if (selected == selection_) return {};
    selection_ = selected;
    return {.selectionChanged = true, .session = session_, .selection = selected};
}
Decision Interaction::escape(bool down) {
    if (escapeHeld_) {
        if (!down) escapeHeld_ = false;
        return {.consumed = true};
    }
    if (!down || !active_) return {};
    escapeHeld_ = true;
    auto result = cancel(); result.consumed = true;
    return result;
}
Decision Interaction::cancel() {
    if (!active_) return {};
    active_ = false;
    return {.hide = true, .session = session_};
}
Decision Interaction::setPaused(bool paused) {
    paused_ = paused;
    return paused ? cancel() : Decision{};
}
}
