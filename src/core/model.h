#pragma once
#include <QPointF>
#include <QMetaType>
#include <QRectF>
#include <QString>
#include <Qt>
#include <array>
#include <optional>
namespace wheel {
enum class Modifier : unsigned { None = 0, Control = 1, Alt = 2, Shift = 4, Meta = 8 };
using Modifiers = unsigned;
constexpr Modifiers bit(Modifier m) { return static_cast<Modifiers>(m); }
enum class MouseButton { Right, Middle, Back, Forward };
enum class Theme { Light, Warm, Dark };
struct Shortcut {
    int key = 0;
    Modifiers modifiers = 0;
    bool operator==(const Shortcut&) const = default;
};
struct Slot {
    QString name;
    Shortcut shortcut;
    bool operator==(const Slot&) const = default;
};
struct Config {
    Modifier modifier = Modifier::None;
    MouseButton button = MouseButton::Middle;
    Theme theme = Theme::Light;
    std::array<Slot, 8> slots;
    bool operator==(const Config&) const = default;
};
Config defaultConfig();
QString validate(const Config& config);
bool supportedKey(int key);
QString shortcutText(const Shortcut& shortcut);
struct Geometry {
    QPointF center;
    double radius = 164;
    double deadRadius = 42;
    static Geometry fit(QPointF cursor, QRectF available, double scale);
    int hit(QPointF position) const;
};
}
Q_DECLARE_METATYPE(wheel::Config)
Q_DECLARE_METATYPE(wheel::Geometry)
