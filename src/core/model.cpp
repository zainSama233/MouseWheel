#include "core/model.h"
#include <QKeySequence>
#include <algorithm>
#include <cmath>
#include <numbers>
#include <QUrl>
#include <QFileInfo>
#include "core/image_asset.h"
namespace wheel {
QString actionKindName(ActionKind kind) {
    switch(kind) {
    case ActionKind::Shortcut: return QStringLiteral("快捷键");
    case ActionKind::Screenshot: return QStringLiteral("截图贴图");
    case ActionKind::ScreenAnnotation: return QStringLiteral("屏幕标注");
    case ActionKind::Application: return QStringLiteral("打开应用");
    case ActionKind::Website: return QStringLiteral("打开网页");
    }
    return {};
}
Config defaultConfig() {
    Config c;
    c.slots[0]={actionKindName(ActionKind::Screenshot),{},ActionKind::Screenshot};
    c.slots[1]={actionKindName(ActionKind::ScreenAnnotation),{},ActionKind::ScreenAnnotation};
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
    if(c.shape<WheelShape::Sector || c.shape>WheelShape::Hexagon) return QStringLiteral("不支持的槽位形状。");
    if(!c.centerImage.isEmpty() && decodeCenterImage(c.centerImage).isNull()) return QStringLiteral("中心图片无效。");
    for(const auto& slot:c.slots) {
        const auto error=validate(slot); if(!error.isEmpty()) return error;
    }
    return {};
}
QString validate(const Slot& slot) {
    if (slot.name.size() > 12) return QStringLiteral("槽位名称最多 12 个字符。");
    if(slot.target.size()>2048) return QStringLiteral("启动地址过长。");
    if(slot.kind==ActionKind::Application || slot.kind==ActionKind::Website) {
        if(slot.name.trimmed().isEmpty() || slot.shortcut.key || slot.shortcut.modifiers)
            return QStringLiteral("启动动作需要名称，不能包含快捷键。");
        if(slot.kind==ActionKind::Application) {
            const QFileInfo file(slot.target);
            if(!file.isAbsolute() || (file.suffix().compare("exe",Qt::CaseInsensitive)!=0 && file.suffix().compare("lnk",Qt::CaseInsensitive)!=0 && file.suffix().compare("app",Qt::CaseInsensitive)!=0))
                return QStringLiteral("请选择应用文件或快捷方式。");
        } else {
            const QUrl url(slot.target,QUrl::StrictMode);
            if(!url.isValid() || url.host().isEmpty() || (url.scheme()!="https" && url.scheme()!="http") || slot.target.contains(QChar(' ')))
                return QStringLiteral("请输入有效的 HTTP 或 HTTPS 网页地址。");
        }
        return {};
    }
    if(!slot.target.isEmpty()) return QStringLiteral("此动作不能包含启动地址。");
    if (slot.kind==ActionKind::Screenshot || slot.kind==ActionKind::ScreenAnnotation) {
        if(slot.name.trimmed().isEmpty() || slot.shortcut.key || slot.shortcut.modifiers)
            return QStringLiteral("内置工具需要名称，不能包含快捷键。");
        return {};
    }
    if(slot.kind!=ActionKind::Shortcut) return QStringLiteral("不支持的动作类型。");
    if (!slot.shortcut.key) {
        if (!slot.name.isEmpty() || slot.shortcut.modifiers)
            return QStringLiteral("空槽位不能包含名称或修饰键。");
    } else if (slot.name.trimmed().isEmpty() || !supportedKey(slot.shortcut.key) ||
               (slot.shortcut.modifiers & ~15u)) {
        return QStringLiteral("请为每个动作填写名称和受支持的快捷键。");
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
    const double r = std::min({WheelRadius * scale, area.width() / 2, area.height() / 2});
    return {{std::clamp(p.x(), area.left()+r, area.right()-r),
             std::clamp(p.y(), area.top()+r, area.bottom()-r)}, r};
}
QPointF slotCenter(int index) {
    const double angle=index*std::numbers::pi/4;
    return {112*std::sin(angle),-112*std::cos(angle)};
}
const QPainterPath& slotPath(WheelShape shape,int index) {
    static const auto paths=[] {
        std::array<std::array<QPainterPath,8>,3> result;
        for(int kind=0;kind<3;++kind) for(int slot=0;slot<8;++slot) {
            auto& path=result[kind][slot]; const auto center=slotCenter(slot);
            if(kind==int(WheelShape::Circle)) path.addEllipse(center,36,36);
            else if(kind==int(WheelShape::Hexagon)) {
                for(int vertex=0;vertex<6;++vertex) {
                    const double angle=vertex*std::numbers::pi/3;
                    const auto point=center+QPointF(39*std::cos(angle),39*std::sin(angle));
                    if(vertex==0) path.moveTo(point); else path.lineTo(point);
                }
                path.closeSubpath();
            } else {
                const QRectF outer(-156,-156,312,312),inner(-66,-66,132,132);
                path.arcMoveTo(outer,110.5-slot*45); path.arcTo(outer,110.5-slot*45,-41);
                path.arcTo(inner,69.5-slot*45,41); path.closeSubpath();
            }
        }
        return result;
    }();
    Q_ASSERT(index>=0 && index<8 && shape>=WheelShape::Sector && shape<=WheelShape::Hexagon);
    return paths[int(shape)][index];
}
int Geometry::hit(QPointF position,WheelShape shape) const {
    if(radius<=0) return -1;
    const auto point=(position-center)*(WheelRadius/radius);
    for(int index=0;index<8;++index) if(slotPath(shape,index).contains(point)) return index;
    return -1;
}
}
