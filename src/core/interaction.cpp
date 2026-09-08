#include "core/interaction.h"
#include <QLineF>
namespace wheel {
Decision Interaction::press(MouseButton b, Modifiers mods, const Config& c,
                            const Geometry& geometry, quintptr target, bool permitted) {
    if (held_ == b) return {.consumed = true};
    if (!permitted || paused_ || held_ || b != c.button || mods != bit(c.modifier) || !target) return {};
    held_ = b; active_ = true; snapshot_ = c; geometry_ = geometry;
    group_=-1;hoverDeadline_=-1;navigationArmed_=true;
    target_ = target; selection_ = -1; ++session_;
    return {.consumed = true, .show = true, .session = session_};
}
Decision Interaction::release(MouseButton b, QPointF position) {
    if (held_ != b) return {};
    held_.reset();
    Decision result{.consumed = true, .session = session_, .target = target_};
    if (!active_) return result;
    result.hide = true;
    const int selected = geometry_.hit(position,snapshot_.shape,slots().size());
    if (navigationArmed_ && selected >= 0 && slots()[selected].enabled() && slots()[selected].kind()!=ActionKind::Group)
        result.action = slots()[selected];
    active_ = false;hoverDeadline_=-1;
    return result;
}
const QList<Slot>& Interaction::slots() const {
    return group_<0?snapshot_.slots:std::get<GroupAction>(snapshot_.slots[group_].action).slots;
}
Decision Interaction::move(QPointF position,qint64 nowMs) {
    if (!active_) return {};
    position_=position;
    if(!navigationArmed_) {
        if(QLineF(position,navigationOrigin_).length()<12*geometry_.radius/WheelRadius) return {};
        navigationArmed_=true;
    }
    if(group_>=0 && QLineF(position,geometry_.center).length()<CenterRadius*geometry_.radius/WheelRadius) {
        group_=-1;selection_=-1;hoverDeadline_=-1;navigationArmed_=false;navigationOrigin_=position;
        return {.levelChanged=true,.session=session_};
    }
    const auto selected = geometry_.hit(position,snapshot_.shape,slots().size());
    if (selected == selection_) return {};
    selection_ = selected;
    hoverDeadline_=group_<0 && selected>=0 && slots()[selected].kind()==ActionKind::Group?nowMs+350:-1;
    return {.selectionChanged = true, .session = session_, .selection = selected};
}
Decision Interaction::advance(qint64 nowMs) {
    if(!active_ || hoverDeadline_<0 || nowMs<hoverDeadline_) return {};
    group_=selection_;selection_=-1;hoverDeadline_=-1;
    navigationArmed_=false;navigationOrigin_=position_;
    return {.levelChanged=true,.session=session_};
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
    active_ = false;hoverDeadline_=-1;
    return {.hide = true, .session = session_};
}
Decision Interaction::setPaused(bool paused) {
    paused_ = paused;
    return paused ? cancel() : Decision{};
}
}
