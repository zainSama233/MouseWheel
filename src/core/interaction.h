#pragma once
#include "core/model.h"
namespace wheel {
struct Decision {
    bool consumed = false;
    bool show = false;
    bool hide = false;
    bool selectionChanged = false;
    quint64 session = 0;
    quintptr target = 0;
    int selection = -1;
    std::optional<Shortcut> action;
};
class Interaction final {
public:
    Decision press(MouseButton button, Modifiers modifiers, const Config& config,
                   const Geometry& geometry, quintptr target);
    Decision release(MouseButton button, QPointF position);
    Decision move(QPointF position);
    Decision escape(bool down);
    Decision cancel();
    Decision setPaused(bool paused);
    bool active() const { return active_; }
    quint64 session() const { return session_; }
    const Config& snapshot() const { return snapshot_; }
    const Geometry& geometry() const { return geometry_; }
private:
    bool paused_ = false;
    bool active_ = false;
    bool escapeHeld_ = false;
    std::optional<MouseButton> held_;
    quint64 session_ = 0;
    quintptr target_ = 0;
    int selection_ = -1;
    Config snapshot_;
    Geometry geometry_;
};
}
