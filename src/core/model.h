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
#include <QColor>
#include <functional>
class QKeySequence;
namespace wheel {
enum class Modifier : unsigned { None = 0, Control = 1, Alt = 2, Shift = 4, Meta = 8 };
using Modifiers = unsigned;
constexpr Modifiers bit(Modifier m) { return static_cast<Modifiers>(m); }
enum class MouseButton { Right, Middle, Back, Forward };
enum class Theme { Light, Warm, Dark, System, Morandi, Ocean };
enum class Language { SimplifiedChinese, TraditionalChinese, English, Japanese };
enum class ContentLayout { Below, Above, Left, Right, IconOnly };
enum class EdgePolicy { Translate, Shrink };
struct SlotStyle {
    std::optional<QColor> fill,glow,border,text;
    std::optional<QString> fontFamily;
    std::optional<int> fontSize,iconSize;
    std::optional<double> offsetX,offsetY,borderWidth,glowRadius;
    std::optional<ContentLayout> layout;
    bool operator==(const SlotStyle&) const = default;
};
struct Shortcut {
    int key = 0;
    Modifiers modifiers = 0;
    bool operator==(const Shortcut&) const = default;
};
enum class ActionKind { Shortcut, Screenshot, ScreenAnnotation, Application, Website, Folder, Command, Ocr, Window, System, Group };
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
enum class Shell { Cmd, PowerShell, Wsl, Zsh };
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
struct Slot;
struct GroupAction {
    GroupAction();
    QList<Slot> slots;
    bool operator==(const GroupAction&) const;
};
using Action=std::variant<Shortcut,ScreenshotAction,AnnotationAction,ApplicationAction,WebsiteAction,FolderAction,CommandAction,OcrAction,WindowAction,SystemAction,GroupAction>;
enum class IconSource { Builtin, Program, Image, Automatic, Library };
struct IconSpec {
    IconSource source=IconSource::Builtin; QString value="keyboard"; QByteArray image;
    bool operator==(const IconSpec&) const = default;
};
struct BuiltinIcon { QString id,title; };
const QList<BuiltinIcon>& builtinIcons();
IconSpec suggestedIcon(const Action& action);

enum class WheelShape { Original, Circle, HexagonHive, Capsule };
inline constexpr double WheelRadius=164;
inline constexpr double CenterRadius=42;
const QPainterPath& slotPath(WheelShape shape,int index,int count=8);
QPointF slotCenter(int index,int count=8,WheelShape shape=WheelShape::Circle);
bool supportedSlotCount(int count);
SlotStyle cascadeStyle(const SlotStyle& base,const SlotStyle& overrides);
struct Slot {
    SlotStyle style;
    QString name;
    Action action=Shortcut{};
    IconSpec icon{IconSource::Automatic,{},{}};
    bool showLabel=false;
    Slot()=default;
    Slot(QString title,Action value) : name(std::move(title)),action(std::move(value)),icon((kind()==ActionKind::Application || kind()==ActionKind::Website)?IconSpec{IconSource::Automatic,{},{}}:suggestedIcon(action)),showLabel(kind()!=ActionKind::Shortcut) {}
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
struct WheelConfig {
    Theme theme=Theme::Light;
    QList<Slot> slots=QList<Slot>(8);
    WheelShape shape=WheelShape::Circle;
    QByteArray centerImage;
    Slot center;
    bool centerEnabled=false;
    double deadZone=CenterRadius;
    SlotStyle style;
    bool frosted=false;
    QPointF safetyMargin;
    EdgePolicy edgePolicy=EdgePolicy::Translate;
    bool operator==(const WheelConfig&) const = default;
};
struct Profile {
    QString id,name;
    QStringList applications;
    WheelConfig wheel;
    bool operator==(const Profile&) const = default;
};
struct IconAsset {
    QString id,name;
    bool operator==(const IconAsset&) const = default;
};
struct ColorPreset {
    QString id,name;
    QColor color;
    bool operator==(const ColorPreset&) const = default;
};
struct Config:WheelConfig {
    TriggerRules triggerRules;
    Modifier modifier=Modifier::None;
    MouseButton button=MouseButton::Middle;
    Language language=Language::SimplifiedChinese;
    QList<Profile> profiles;
    QList<IconAsset> assets;
    QList<ColorPreset> colors;
    Config resolved(const QString& executable) const;
    bool operator==(const Config&) const = default;
};
QString executableIdentity(const QString& path);
bool applicationPath(const QString& path);
void visitSlots(Config& config,const std::function<void(Slot&)>& visitor);
QString validate(const SlotStyle& style);
QString validate(const WheelConfig& config);
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
    double extent = WheelRadius;
    int hit(QPointF position,WheelShape shape=WheelShape::Circle,int count=8) const;
};
}
Q_DECLARE_METATYPE(wheel::Config)
Q_DECLARE_METATYPE(wheel::Geometry)

Q_DECLARE_METATYPE(wheel::ActionKind)

Q_DECLARE_METATYPE(wheel::Slot)

Q_DECLARE_METATYPE(wheel::Shortcut)
