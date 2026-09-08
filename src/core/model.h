#pragma once
#include <QPointF>
#include <QMetaType>
#include <QRectF>
#include <QString>
#include <Qt>
#include <array>
#include <QPainterPath>
#include <QByteArray>
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
enum class ActionKind { Shortcut, Screenshot, ScreenAnnotation, Application, Website };
enum class WheelShape { Sector, Circle, Hexagon };
inline constexpr double WheelRadius=164;
inline constexpr double CenterRadius=42;
const QPainterPath& slotPath(WheelShape shape,int index);
QPointF slotCenter(int index);
struct Slot {
    QString name;
    Shortcut shortcut;
    ActionKind kind=ActionKind::Shortcut;
    QString target;
    bool enabled() const { return kind!=ActionKind::Shortcut || shortcut.key!=0; }
    bool operator==(const Slot&) const = default;
};
struct Config {
    Modifier modifier = Modifier::None;
    MouseButton button = MouseButton::Middle;
    Theme theme = Theme::Light;
    std::array<Slot, 8> slots;
    WheelShape shape=WheelShape::Circle;
    QByteArray centerImage;
    bool operator==(const Config&) const = default;
};
QString actionKindName(ActionKind kind);
Config defaultConfig();
QString validate(const Config& config);
QString validate(const Slot& slot);
bool supportedKey(int key);
QString shortcutText(const Shortcut& shortcut);
struct Geometry {
    QPointF center;
    double radius = WheelRadius;
    static Geometry fit(QPointF cursor, QRectF available, double scale);
    int hit(QPointF position,WheelShape shape=WheelShape::Circle) const;
};
}
Q_DECLARE_METATYPE(wheel::Config)
Q_DECLARE_METATYPE(wheel::Geometry)

Q_DECLARE_METATYPE(wheel::ActionKind)

Q_DECLARE_METATYPE(wheel::Slot)
