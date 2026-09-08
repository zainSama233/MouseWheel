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
#include <variant>
#include <QList>
#include <QStringList>
class QKeySequence;
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
enum class ActionKind { Shortcut, Screenshot, ScreenAnnotation, Application, Website, Folder, Command, Ocr, Window, System };
struct ScreenshotAction { bool operator==(const ScreenshotAction&) const = default; };
struct AnnotationAction { bool operator==(const AnnotationAction&) const = default; };
struct ApplicationAction {
    QString path, arguments, directory; bool normalUser=true;
    bool operator==(const ApplicationAction&) const = default;
};
enum class Browser { Default, Chrome, Edge, Firefox, Custom };
struct WebsiteAction {
    QString url; Browser browser=Browser::Default; QString executable;
    bool operator==(const WebsiteAction&) const = default;
};
enum class FolderLocation { Path, Desktop, Downloads, Documents, Pictures, Home, Computer, RecycleBin };
struct FolderAction {
    FolderLocation location=FolderLocation::Desktop; QString path;
    bool operator==(const FolderAction&) const = default;
};
enum class Shell { Cmd, PowerShell, Wsl };
struct CommandAction {
    Shell shell=Shell::PowerShell; QString script, directory; bool hidden=true;
    bool operator==(const CommandAction&) const = default;
};
enum class OcrProvider { Local, Ai, Http };
struct OcrAction {
    OcrProvider provider=OcrProvider::Local; QString endpoint, apiKey, model, resultPath="text";
    bool operator==(const OcrAction&) const = default;
};
enum class WindowOperation { Switch, TileLeft, TileRight, NextMonitor, Topmost, Opacity, Maximize, Minimize };
struct WindowAction {
    WindowOperation operation=WindowOperation::Topmost; int opacity=85;
    bool operator==(const WindowAction&) const = default;
};
enum class SystemOperation { Lock, VolumeUp, VolumeDown, Mute, PlayPause, NextTrack, PreviousTrack, TaskView, DesktopLeft, DesktopRight, NewDesktop, CloseDesktop, ShowDesktop };
struct SystemAction {
    SystemOperation operation=SystemOperation::Mute;
    bool operator==(const SystemAction&) const = default;
};
using Action=std::variant<Shortcut,ScreenshotAction,AnnotationAction,ApplicationAction,WebsiteAction,FolderAction,CommandAction,OcrAction,WindowAction,SystemAction>;
enum class IconSource { Builtin, Program, Image };
struct IconSpec {
    IconSource source=IconSource::Builtin; QString value="keyboard"; QByteArray image;
    bool operator==(const IconSpec&) const = default;
};
struct BuiltinIcon { QString id,title; };
const QList<BuiltinIcon>& builtinIcons();
IconSpec suggestedIcon(const Action& action);

enum class WheelShape { Sector, Circle, Hexagon };
inline constexpr double WheelRadius=164;
inline constexpr double CenterRadius=42;
const QPainterPath& slotPath(WheelShape shape,int index);
QPointF slotCenter(int index);
struct Slot {
    QString name;
    Action action=Shortcut{};
    IconSpec icon;
    bool showLabel=false;
    Slot()=default;
    Slot(QString title,Action value) : name(std::move(title)),action(std::move(value)),icon(suggestedIcon(action)),showLabel(kind()!=ActionKind::Shortcut) {}
    ActionKind kind() const { return static_cast<ActionKind>(action.index()); }
    bool enabled() const { const auto* key=std::get_if<Shortcut>(&action); return !key || key->key!=0; }
    bool operator==(const Slot&) const = default;
};
struct TriggerRules {
    bool pauseFullscreen=false;
    QStringList excludedApplications;
    bool allows(const QString& executable,bool fullscreen) const;
    bool operator==(const TriggerRules&) const = default;
};
struct Config {
    TriggerRules triggerRules;
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
QString validate(const Action& action);
bool supportedKey(int key);
QKeySequence shortcutSequence(const Shortcut& shortcut);
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

Q_DECLARE_METATYPE(wheel::Shortcut)
