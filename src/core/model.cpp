#include "core/model.h"
#include <QDir>
#include <QKeySequence>
#include <algorithm>
#include <cmath>
#include <numbers>
#include <QUrl>
#include <QFileInfo>
#include <QStringList>
#include "core/image_asset.h"
namespace wheel {
QString actionKindName(ActionKind kind) {
    switch(kind) {
    case ActionKind::Shortcut: return QStringLiteral("快捷键");
    case ActionKind::Screenshot: return QStringLiteral("截图贴图");
    case ActionKind::ScreenAnnotation: return QStringLiteral("屏幕标注");
    case ActionKind::Application: return QStringLiteral("打开应用");
    case ActionKind::Website: return QStringLiteral("打开网址");
    case ActionKind::Folder: return QStringLiteral("打开文件夹");
    case ActionKind::Command: return QStringLiteral("运行命令");
    case ActionKind::Ocr: return QStringLiteral("屏幕 OCR");
    case ActionKind::Window: return QStringLiteral("窗口管理");
    case ActionKind::System: return QStringLiteral("系统控制");
    }
    return {};
}
Config defaultConfig() {
    Config c;
    c.slots[0]={actionKindName(ActionKind::Screenshot),ScreenshotAction{}};
    c.slots[1]={actionKindName(ActionKind::ScreenAnnotation),AnnotationAction{}};
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
           key == Qt::Key_Escape || key == Qt::Key_Pause || key == Qt::Key_Cancel;
}
bool TriggerRules::allows(const QString& executable,bool fullscreen) const {
    if(pauseFullscreen && fullscreen) return false;
    const auto path=QDir::cleanPath(QDir::fromNativeSeparators(executable));
    for(const auto& excluded:excludedApplications)
        if(path.compare(QDir::cleanPath(QDir::fromNativeSeparators(excluded)),Qt::CaseInsensitive)==0) return false;
    return true;
}
QString validate(const Config& c) {
    if(c.triggerRules.excludedApplications.size()>256) return QStringLiteral("暂停应用最多为 256 个。");
    for(const auto& path:c.triggerRules.excludedApplications)
        if(path.isEmpty() || path.size()>32767 || path.contains(QChar::Null) || !QDir::isAbsolutePath(path) || !path.endsWith(".exe",Qt::CaseInsensitive))
            return QStringLiteral("请选择暂停轮盘的应用程序（完整 EXE 路径）。");
    if (c.modifier != Modifier::None && c.modifier != Modifier::Control && c.modifier != Modifier::Alt &&
        c.modifier != Modifier::Shift && c.modifier != Modifier::Meta)
        return QStringLiteral("请选择一个触发修饰键。");
    if (c.button < MouseButton::Right || c.button > MouseButton::Forward ||
        c.theme < Theme::Light || c.theme > Theme::Dark)
        return QStringLiteral("配置包含不支持的选项。");
    if (c.modifier == Modifier::None && c.button != MouseButton::Middle)
        return QStringLiteral("单键触发使用鼠标中键。");
    if(c.shape<WheelShape::Sector || c.shape>WheelShape::Hexagon) return QStringLiteral("不支持的槽位形状。");
    if(!c.centerImage.isEmpty() && decodeImageAsset(c.centerImage).isNull()) return QStringLiteral("中心图片无效。");
    for(const auto& slot:c.slots) {
        const auto error=validate(slot); if(!error.isEmpty()) return error;
    }
    return {};
}
const QList<BuiltinIcon>& builtinIcons() {
    static const QList<BuiltinIcon> icons{
        {"keyboard",QStringLiteral("键盘")},{"copy",QStringLiteral("复制")},{"clipboard-paste",QStringLiteral("粘贴")},
        {"scissors",QStringLiteral("剪切")},{"undo-2",QStringLiteral("撤销")},{"redo-2",QStringLiteral("重做")},
        {"scan",QStringLiteral("识别")},{"save",QStringLiteral("保存")},{"camera",QStringLiteral("截图")},
        {"pencil",QStringLiteral("画笔")},{"globe",QStringLiteral("网页")},{"app-window",QStringLiteral("窗口")},
        {"folder",QStringLiteral("文件夹")},{"terminal",QStringLiteral("终端")},{"settings",QStringLiteral("设置")},
        {"x",QStringLiteral("取消")},{"plus",QStringLiteral("添加")}};
    return icons;
}
IconSpec suggestedIcon(const Action& action) {
    QString name="keyboard";
    switch(static_cast<ActionKind>(action.index())) {
    case ActionKind::Shortcut: {
        const auto s=std::get<Shortcut>(action);
        if(s.modifiers==bit(Modifier::Control)) {
            switch(s.key) {
            case Qt::Key_C: name="copy"; break; case Qt::Key_V: name="clipboard-paste"; break;
            case Qt::Key_X: name="scissors"; break; case Qt::Key_Z: name="undo-2"; break;
            case Qt::Key_Y: name="redo-2"; break; case Qt::Key_A: name="scan"; break; case Qt::Key_S: name="save"; break;
            }
        }
        break;
    }
    case ActionKind::Screenshot: name="camera"; break;
    case ActionKind::ScreenAnnotation: name="pencil"; break;
    case ActionKind::Application: return {IconSource::Program,std::get<ApplicationAction>(action).path,{}};
    case ActionKind::Website: name="globe"; break;
    case ActionKind::Folder: name="folder"; break;
    case ActionKind::Command: name="terminal"; break;
    case ActionKind::Ocr: name="scan"; break;
    case ActionKind::Window: name="app-window"; break;
    case ActionKind::System: name="settings"; break;
    }
    return {IconSource::Builtin,name,{}};
}
QString validate(const Slot& slot) {
    if(slot.name.size()>12 || (slot.enabled() && slot.name.trimmed().isEmpty())) return QStringLiteral("请填写最多 12 字的名称。");
    if(slot.icon.source<IconSource::Builtin || slot.icon.source>IconSource::Automatic) return QStringLiteral("图标来源无效。");
    if(slot.icon.source==IconSource::Automatic && !slot.icon.image.isEmpty() && decodeImageAsset(slot.icon.image).isNull()) return QStringLiteral("自动图标缓存无效。");
    if(slot.icon.source==IconSource::Image && decodeImageAsset(slot.icon.image).isNull()) return QStringLiteral("槽位图片无效。");
    if(slot.icon.source==IconSource::Program && !QFileInfo(slot.icon.value).isAbsolute()) return QStringLiteral("请选择图标来源程序。");
    if(slot.icon.source==IconSource::Builtin && std::none_of(builtinIcons().begin(),builtinIcons().end(),[&](const auto& icon){return icon.id==slot.icon.value;})) return QStringLiteral("内置图标无效。");
    if(!slot.enabled() && !slot.name.isEmpty()) return QStringLiteral("空槽位不能包含名称。");
    return validate(slot.action);
}
QString validate(const Action& action) {
    const auto validUrl=[](const QString& text) { const QUrl url(text,QUrl::StrictMode); return text.size()<=2048 && !text.contains(' ') && url.isValid() && !url.host().isEmpty() && (url.scheme()=="https" || url.scheme()=="http"); };
    return std::visit([&](const auto& a)->QString {
        using T=std::decay_t<decltype(a)>;
        if constexpr(std::is_same_v<T,Shortcut>) {
            if((!a.key && a.modifiers) || (a.key && (!supportedKey(a.key) || (a.modifiers&~15u)))) return QStringLiteral("快捷键无效。");
        } else if constexpr(std::is_same_v<T,ApplicationAction>) {
            if(!QFileInfo(a.path).isAbsolute() || a.path.size()>2048 || a.arguments.size()>8192 || (!a.directory.isEmpty() && !QFileInfo(a.directory).isAbsolute())) return QStringLiteral("请选择文件及有效工作目录。");
        } else if constexpr(std::is_same_v<T,WebsiteAction>) {
            if(!validUrl(a.url) || a.browser<Browser::Default || a.browser>Browser::Custom || (a.browser==Browser::Custom && !QFileInfo(a.executable).isAbsolute())) return QStringLiteral("请输入网页地址并选择浏览器。");
        } else if constexpr(std::is_same_v<T,FolderAction>) {
            if(a.location<FolderLocation::Path || a.location>FolderLocation::RecycleBin || (a.location==FolderLocation::Path && !QFileInfo(a.path).isAbsolute())) return QStringLiteral("请选择有效目录。");
        } else if constexpr(std::is_same_v<T,CommandAction>) {
            if(a.shell<Shell::Cmd || a.shell>Shell::Wsl || a.script.trimmed().isEmpty() || a.script.size()>32768 || (!a.directory.isEmpty() && !QFileInfo(a.directory).isAbsolute())) return QStringLiteral("请输入命令及有效工作目录。");
        } else if constexpr(std::is_same_v<T,OcrAction>) {
            if(a.provider<OcrProvider::Local || a.provider>OcrProvider::Http || (a.provider!=OcrProvider::Local && !validUrl(a.endpoint)) || (a.provider==OcrProvider::Ai && a.model.trimmed().isEmpty()) || (a.provider==OcrProvider::Http && a.resultPath.trimmed().isEmpty())) return QStringLiteral("请填写识别服务地址及模型／结果字段。");
        } else if constexpr(std::is_same_v<T,WindowAction>) {
            if(a.operation<WindowOperation::Switch || a.operation>WindowOperation::Minimize || a.opacity<20 || a.opacity>100) return QStringLiteral("窗口操作或透明度无效。");
        } else if constexpr(std::is_same_v<T,SystemAction>) {
            if(a.operation<SystemOperation::Lock || a.operation>SystemOperation::ShowDesktop) return QStringLiteral("系统操作无效。");
        }
        return {};
    },action);
}
QKeySequence shortcutSequence(const Shortcut& s) {
    if (!s.key) return {};
    Qt::KeyboardModifiers mods;
    if (s.modifiers & bit(Modifier::Control)) mods |= Qt::ControlModifier;
    if (s.modifiers & bit(Modifier::Alt)) mods |= Qt::AltModifier;
    if (s.modifiers & bit(Modifier::Shift)) mods |= Qt::ShiftModifier;
    if (s.modifiers & bit(Modifier::Meta)) mods |= Qt::MetaModifier;
    return QKeySequence(QKeyCombination(mods, static_cast<Qt::Key>(s.key)));
}
QString shortcutText(const Shortcut& s) {return shortcutSequence(s).toString(QKeySequence::NativeText);}
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
