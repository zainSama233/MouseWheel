#include <QCoreApplication>
#include "core/model.h"
#include <QDir>
#include <QKeySequence>
#include <algorithm>
#include <cmath>
#include <numbers>
#include <QUrl>
#include <QFileInfo>
#include <QStringList>
#include <QSet>
#include <QRegularExpression>
#include "core/image_asset.h"
namespace wheel {
GroupAction::GroupAction():slots(8) {}
bool GroupAction::operator==(const GroupAction&) const = default;
bool supportedSlotCount(int count) { return count==4 || count==8 || count==12; }
QString actionKindName(ActionKind kind) {
    switch(kind) {
    case ActionKind::Shortcut: return QCoreApplication::translate("MouseWheel","快捷键");
    case ActionKind::Screenshot: return QCoreApplication::translate("MouseWheel","截图贴图");
    case ActionKind::ScreenAnnotation: return QCoreApplication::translate("MouseWheel","屏幕标注");
    case ActionKind::Application: return QCoreApplication::translate("MouseWheel","打开应用");
    case ActionKind::Website: return QCoreApplication::translate("MouseWheel","打开网址");
    case ActionKind::Folder: return QCoreApplication::translate("MouseWheel","打开文件夹");
    case ActionKind::Command: return QCoreApplication::translate("MouseWheel","运行命令");
    case ActionKind::Ocr: return QCoreApplication::translate("MouseWheel","屏幕 OCR");
    case ActionKind::Window: return QCoreApplication::translate("MouseWheel","窗口管理");
    case ActionKind::Group: return QCoreApplication::translate("MouseWheel","子轮盘");
    case ActionKind::System: return QCoreApplication::translate("MouseWheel","系统控制");
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
QString executableIdentity(const QString& path){return QDir::cleanPath(QDir::fromNativeSeparators(path)).toCaseFolded();}
bool TriggerRules::allows(const QString& executable,bool fullscreen) const {
    if(pauseFullscreen && fullscreen) return false;
    const auto path=executableIdentity(executable);
    for(const auto& excluded:excludedApplications)
        if(path==executableIdentity(excluded)) return false;
    return true;
}
QString validate(const Config& c) {
    if(c.triggerRules.excludedApplications.size()>256) return QCoreApplication::translate("MouseWheel","暂停应用最多为 256 个。");
    for(const auto& path:c.triggerRules.excludedApplications)
        if(path.isEmpty() || path.size()>32767 || path.contains(QChar::Null) || !QDir::isAbsolutePath(path) || !path.endsWith(".exe",Qt::CaseInsensitive))
            return QCoreApplication::translate("MouseWheel","请选择暂停轮盘的应用程序（完整 EXE 路径）。");
    if (c.modifier != Modifier::None && c.modifier != Modifier::Control && c.modifier != Modifier::Alt &&
        c.modifier != Modifier::Shift && c.modifier != Modifier::Meta)
        return QCoreApplication::translate("MouseWheel","请选择一个触发修饰键。");
    if (c.button < MouseButton::Right || c.button > MouseButton::Forward ||
        c.theme < Theme::Light || c.theme > Theme::Ocean)
        return QCoreApplication::translate("MouseWheel","配置包含不支持的选项。");
    if (c.modifier == Modifier::None && c.button != MouseButton::Middle)
        return QCoreApplication::translate("MouseWheel","单键触发使用鼠标中键。");
    if(c.language<Language::SimplifiedChinese || c.language>Language::Japanese) return QCoreApplication::translate("MouseWheel","语言无效。");
    if(c.profiles.size()>64 || c.assets.size()>1024 || c.colors.size()>256) return QCoreApplication::translate("MouseWheel","配置条目过多。");
    QSet<QString> ids,applications,assets;
    for(const auto& profile:c.profiles) {
        if(profile.id.isEmpty() || profile.id=="global" || ids.contains(profile.id) || profile.name.trimmed().isEmpty() || profile.name.size()>64) return QCoreApplication::translate("MouseWheel","方案名称或标识无效。");
        ids.insert(profile.id);
        const auto error=validate(profile.wheel);if(!error.isEmpty()) return error;
        for(const auto& path:profile.applications) {
            const auto normalized=executableIdentity(path);
            if(!QDir::isAbsolutePath(path) || !path.endsWith(".exe",Qt::CaseInsensitive) || applications.contains(normalized)) return QCoreApplication::translate("MouseWheel","应用路径无效或重复绑定。");
            applications.insert(normalized);
        }
    }
    static const QRegularExpression assetId("^[a-f0-9]{64}\\.(svg|png|ico|jpg)$");
    for(const auto& asset:c.assets) {
        if(!assetId.match(asset.id).hasMatch() || assets.contains(asset.id) || asset.name.trimmed().isEmpty() || asset.name.size()>64) return QCoreApplication::translate("MouseWheel","图标库条目无效。");
        assets.insert(asset.id);
    }
    ids.clear();
    for(const auto& color:c.colors) {
        if(color.id.isEmpty() || ids.contains(color.id) || color.name.trimmed().isEmpty() || color.name.size()>64 || !color.color.isValid()) return QCoreApplication::translate("MouseWheel","颜色预设无效。");
        ids.insert(color.id);
    }
    Config copy=c;bool missing=false;visitSlots(copy,[&](Slot& slot){if(slot.icon.source==IconSource::Library && !assets.contains(slot.icon.value)) missing=true;});
    if(missing) return QCoreApplication::translate("MouseWheel","槽位引用的图标不存在。");
    return validate(static_cast<const WheelConfig&>(c));
}
QString validate(const WheelConfig& c) {
    if(c.theme<Theme::Light || c.theme>Theme::Ocean || c.shape<WheelShape::Original || c.shape>WheelShape::Capsule) return QCoreApplication::translate("MouseWheel","轮盘外观选项无效。");
    if(!std::isfinite(c.deadZone) || c.deadZone<12 || c.deadZone>52 || c.center.kind()==ActionKind::Group) return QCoreApplication::translate("MouseWheel","中心动作或死区无效。");
    if(!std::isfinite(c.safetyMargin.x()) || !std::isfinite(c.safetyMargin.y()) || c.safetyMargin.x()<0 || c.safetyMargin.y()<0 || c.safetyMargin.x()>300 || c.safetyMargin.y()>300 || c.edgePolicy<EdgePolicy::Translate || c.edgePolicy>EdgePolicy::Shrink) return QCoreApplication::translate("MouseWheel","屏幕安全边距无效。");
    if(!c.centerImage.isEmpty() && decodeImageAsset(c.centerImage).isNull()) return QCoreApplication::translate("MouseWheel","中心图片无效。");
    if(!supportedSlotCount(c.slots.size())) return QCoreApplication::translate("MouseWheel","轮盘支持 4、8 或 12 个槽位。");
    auto error=validate(c.style);if(!error.isEmpty()) return error;
    error=validate(c.center);if(!error.isEmpty()) return error;
    for(const auto& slot:c.slots) {error=validate(slot);if(!error.isEmpty()) return error;}
    return {};
}
SlotStyle cascadeStyle(const SlotStyle& base,const SlotStyle& override) {
    SlotStyle result=base;
    if(override.fill)result.fill=override.fill;
    if(override.glow)result.glow=override.glow;
    if(override.border)result.border=override.border;
    if(override.text)result.text=override.text;
    if(override.fontFamily)result.fontFamily=override.fontFamily;
    if(override.fontSize)result.fontSize=override.fontSize;
    if(override.iconSize)result.iconSize=override.iconSize;
    if(override.offsetX)result.offsetX=override.offsetX;
    if(override.offsetY)result.offsetY=override.offsetY;
    if(override.borderWidth)result.borderWidth=override.borderWidth;
    if(override.glowRadius)result.glowRadius=override.glowRadius;
    if(override.layout)result.layout=override.layout;
    return result;
}
QString validate(const SlotStyle& s) {
    for(const auto& color:{s.fill,s.glow,s.border,s.text}) if(color && !color->isValid()) return QCoreApplication::translate("MouseWheel","颜色无效。");
    if(s.fontFamily && s.fontFamily->size()>128) return QCoreApplication::translate("MouseWheel","字体名称无效。");
    if((s.fontSize && (*s.fontSize<8 || *s.fontSize>48)) || (s.iconSize && (*s.iconSize<12 || *s.iconSize>96))) return QCoreApplication::translate("MouseWheel","字体或图标尺寸无效。");
    for(const auto& value:{s.offsetX,s.offsetY}) if(value && (!std::isfinite(*value) || std::abs(*value)>128)) return QCoreApplication::translate("MouseWheel","偏移超出范围。");
    if(s.borderWidth && (!std::isfinite(*s.borderWidth) || *s.borderWidth<0 || *s.borderWidth>8)) return QCoreApplication::translate("MouseWheel","边框宽度无效。");
    if(s.glowRadius && (!std::isfinite(*s.glowRadius) || *s.glowRadius<0 || *s.glowRadius>24)) return QCoreApplication::translate("MouseWheel","光晕尺寸无效。");
    if(s.layout && (*s.layout<ContentLayout::Below || *s.layout>ContentLayout::IconOnly)) return QCoreApplication::translate("MouseWheel","图文布局无效。");
    return {};
}
Config Config::resolved(const QString& executable) const {
    Config result=*this;
    const auto path=executableIdentity(executable);
    for(const auto& profile:profiles) for(const auto& app:profile.applications)
        if(path==executableIdentity(app)) {static_cast<WheelConfig&>(result)=profile.wheel;return result;}
    return result;
}
void visitSlots(Config& c,const std::function<void(Slot&)>& visitor) {
    const auto visitWheel=[&](WheelConfig& wheel){
        visitor(wheel.center);
        for(auto& slot:wheel.slots) {visitor(slot);if(auto* group=std::get_if<GroupAction>(&slot.action)) for(auto& child:group->slots) visitor(child);}
    };
    visitWheel(c);for(auto& profile:c.profiles) visitWheel(profile.wheel);
}
const QList<BuiltinIcon>& builtinIcons() {
    static const QList<BuiltinIcon> icons{
        {"keyboard",QCoreApplication::translate("MouseWheel","键盘")},{"copy",QCoreApplication::translate("MouseWheel","复制")},{"clipboard-paste",QCoreApplication::translate("MouseWheel","粘贴")},
        {"scissors",QCoreApplication::translate("MouseWheel","剪切")},{"undo-2",QCoreApplication::translate("MouseWheel","撤销")},{"redo-2",QCoreApplication::translate("MouseWheel","重做")},
        {"scan",QCoreApplication::translate("MouseWheel","识别")},{"save",QCoreApplication::translate("MouseWheel","保存")},{"camera",QCoreApplication::translate("MouseWheel","截图")},
        {"pencil",QCoreApplication::translate("MouseWheel","画笔")},{"globe",QCoreApplication::translate("MouseWheel","网页")},{"app-window",QCoreApplication::translate("MouseWheel","窗口")},
        {"folder",QCoreApplication::translate("MouseWheel","文件夹")},{"terminal",QCoreApplication::translate("MouseWheel","终端")},{"settings",QCoreApplication::translate("MouseWheel","设置")},
        {"lock-keyhole",QCoreApplication::translate("MouseWheel","锁屏")},
        {"volume-2",QCoreApplication::translate("MouseWheel","音量增加")},
        {"volume-1",QCoreApplication::translate("MouseWheel","音量降低")},
        {"volume-x",QCoreApplication::translate("MouseWheel","静音")},
        {"circle-play",QCoreApplication::translate("MouseWheel","播放／暂停")},
        {"skip-forward",QCoreApplication::translate("MouseWheel","下一首")},
        {"skip-back",QCoreApplication::translate("MouseWheel","上一首")},
        {"panels-top-left",QCoreApplication::translate("MouseWheel","任务视图")},
        {"panel-left-close",QCoreApplication::translate("MouseWheel","上一个虚拟桌面")},
        {"panel-right-close",QCoreApplication::translate("MouseWheel","下一个虚拟桌面")},
        {"square-plus",QCoreApplication::translate("MouseWheel","新建虚拟桌面")},
        {"monitor-x",QCoreApplication::translate("MouseWheel","关闭虚拟桌面")},
        {"monitor",QCoreApplication::translate("MouseWheel","显示桌面")},
        {"x",QCoreApplication::translate("MouseWheel","取消")},{"plus",QCoreApplication::translate("MouseWheel","添加")}};
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
    case ActionKind::Group:
    case ActionKind::Folder: name="folder"; break;
    case ActionKind::Command: name="terminal"; break;
    case ActionKind::Ocr: name="scan"; break;
    case ActionKind::Window: name="app-window"; break;
    case ActionKind::System:
        switch(std::get<SystemAction>(action).operation) {
        case SystemOperation::Lock: name="lock-keyhole"; break;
        case SystemOperation::VolumeUp: name="volume-2"; break;
        case SystemOperation::VolumeDown: name="volume-1"; break;
        case SystemOperation::Mute: name="volume-x"; break;
        case SystemOperation::PlayPause: name="circle-play"; break;
        case SystemOperation::NextTrack: name="skip-forward"; break;
        case SystemOperation::PreviousTrack: name="skip-back"; break;
        case SystemOperation::TaskView: name="panels-top-left"; break;
        case SystemOperation::DesktopLeft: name="panel-left-close"; break;
        case SystemOperation::DesktopRight: name="panel-right-close"; break;
        case SystemOperation::NewDesktop: name="square-plus"; break;
        case SystemOperation::CloseDesktop: name="monitor-x"; break;
        case SystemOperation::ShowDesktop: name="monitor"; break;
        }
        break;
    }
    return {IconSource::Builtin,name,{}};
}
QString validate(const Slot& slot) {
    const auto styleError=validate(slot.style);if(!styleError.isEmpty()) return styleError;
    if(slot.name.size()>64 || (slot.enabled() && slot.name.trimmed().isEmpty())) return QCoreApplication::translate("MouseWheel","请填写最多 64 字的名称。");
    if(slot.icon.source<IconSource::Builtin || slot.icon.source>IconSource::Library) return QCoreApplication::translate("MouseWheel","图标来源无效。");
    if(slot.icon.source==IconSource::Automatic && !slot.icon.image.isEmpty() && decodeImageAsset(slot.icon.image).isNull()) return QCoreApplication::translate("MouseWheel","自动图标缓存无效。");
    if(slot.icon.source==IconSource::Image && decodeImageAsset(slot.icon.image).isNull()) return QCoreApplication::translate("MouseWheel","槽位图片无效。");
    if(slot.icon.source==IconSource::Program && !QFileInfo(slot.icon.value).isAbsolute()) return QCoreApplication::translate("MouseWheel","请选择图标来源程序。");
    if(slot.icon.source==IconSource::Builtin && std::none_of(builtinIcons().begin(),builtinIcons().end(),[&](const auto& icon){return icon.id==slot.icon.value;})) return QCoreApplication::translate("MouseWheel","内置图标无效。");
    if(!slot.enabled() && !slot.name.isEmpty()) return QCoreApplication::translate("MouseWheel","空槽位不能包含名称。");
    return validate(slot.action);
}
QString validate(const Action& action) {
    const auto validUrl=[](const QString& text) { const QUrl url(text,QUrl::StrictMode); return text.size()<=2048 && !text.contains(' ') && url.isValid() && !url.host().isEmpty() && (url.scheme()=="https" || url.scheme()=="http"); };
    return std::visit([&](const auto& a)->QString {
        using T=std::decay_t<decltype(a)>;
        if constexpr(std::is_same_v<T,GroupAction>) {
            if(!supportedSlotCount(a.slots.size())) return QCoreApplication::translate("MouseWheel","子轮盘支持 4、8 或 12 个槽位。");
            for(const auto& child:a.slots) {
                if(child.kind()==ActionKind::Group) return QCoreApplication::translate("MouseWheel","子轮盘不能再嵌套分组。");
                const auto error=validate(child); if(!error.isEmpty()) return error;
            }
        } else if constexpr(std::is_same_v<T,Shortcut>) {
            if((!a.key && a.modifiers) || (a.key && (!supportedKey(a.key) || (a.modifiers&~15u)))) return QCoreApplication::translate("MouseWheel","快捷键无效。");
        } else if constexpr(std::is_same_v<T,ApplicationAction>) {
            if(!QFileInfo(a.path).isAbsolute() || a.path.size()>2048 || a.arguments.size()>8192 || (!a.directory.isEmpty() && !QFileInfo(a.directory).isAbsolute())) return QCoreApplication::translate("MouseWheel","请选择文件及有效工作目录。");
        } else if constexpr(std::is_same_v<T,WebsiteAction>) {
            if(!validUrl(a.url) || a.browser<Browser::Default || a.browser>Browser::Custom || (a.browser==Browser::Custom && !QFileInfo(a.executable).isAbsolute())) return QCoreApplication::translate("MouseWheel","请输入网页地址并选择浏览器。");
        } else if constexpr(std::is_same_v<T,FolderAction>) {
            if(a.location<FolderLocation::Path || a.location>FolderLocation::RecycleBin || (a.location==FolderLocation::Path && !QFileInfo(a.path).isAbsolute())) return QCoreApplication::translate("MouseWheel","请选择有效目录。");
        } else if constexpr(std::is_same_v<T,CommandAction>) {
            if(a.shell<Shell::Cmd || a.shell>Shell::Wsl || a.script.trimmed().isEmpty() || a.script.size()>32768 || (!a.directory.isEmpty() && !QFileInfo(a.directory).isAbsolute())) return QCoreApplication::translate("MouseWheel","请输入命令及有效工作目录。");
        } else if constexpr(std::is_same_v<T,OcrAction>) {
            if(a.provider<OcrProvider::Local || a.provider>OcrProvider::Http || (a.provider!=OcrProvider::Local && !validUrl(a.endpoint)) || (a.provider==OcrProvider::Ai && a.model.trimmed().isEmpty()) || (a.provider==OcrProvider::Http && a.resultPath.trimmed().isEmpty())) return QCoreApplication::translate("MouseWheel","请填写识别服务地址及模型／结果字段。");
        } else if constexpr(std::is_same_v<T,WindowAction>) {
            if(a.operation<WindowOperation::Switch || a.operation>WindowOperation::Minimize || a.opacity<20 || a.opacity>100) return QCoreApplication::translate("MouseWheel","窗口操作或透明度无效。");
        } else if constexpr(std::is_same_v<T,SystemAction>) {
            if(a.operation<SystemOperation::Lock || a.operation>SystemOperation::ShowDesktop) return QCoreApplication::translate("MouseWheel","系统操作无效。");
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
QPointF slotCenter(int index,int count,WheelShape shape) {
    if(shape==WheelShape::HexagonHive) {
        static const std::array<QPointF,12> axial{{{1,-2},{2,-2},{2,-1},{2,0},{1,1},{0,2},{-1,2},{-2,2},{-2,1},{-2,0},{-1,-1},{0,-2}}};
        const auto cell=axial[index*12/count];
        return {std::sqrt(3.0)*32*(cell.x()+cell.y()/2),48*cell.y()};
    }
    const double angle=index*2*std::numbers::pi/count;
    const double distance=count==12?120:112;
    return {distance*std::sin(angle),-distance*std::cos(angle)};
}
const QPainterPath& slotPath(WheelShape shape,int index,int count) {
    static const auto paths=[] {
        std::array<std::array<std::array<QPainterPath,12>,3>,4> result;
        for(int kind=0;kind<4;++kind) for(int countIndex=0;countIndex<3;++countIndex) {
            const int count=(countIndex+1)*4;
            for(int slot=0;slot<count;++slot) {
                auto& path=result[kind][countIndex][slot];const auto shape=WheelShape(kind);
                const auto center=slotCenter(slot,count,shape);
                if(shape==WheelShape::Circle) {
                    const double r=count==12?27:36;path.addEllipse(center,r,r);
                } else if(shape==WheelShape::HexagonHive) {
                    for(int vertex=0;vertex<6;++vertex) {
                        const double angle=(30+vertex*60)*std::numbers::pi/180;
                        const auto point=center+QPointF(30*std::cos(angle),30*std::sin(angle));
                        if(vertex==0) path.moveTo(point); else path.lineTo(point);
                    }
                    path.closeSubpath();
                } else if(shape==WheelShape::Capsule) {
                    const double width=count==12?42:58;
                    path.addRoundedRect(QRectF(-width/2,-156,width,88),width/2,width/2);
                    QTransform transform;transform.rotate(slot*360.0/count);path=transform.map(path);
                } else {
                    const double step=360.0/count,gap=1.5;
                    const QRectF outer(-156,-156,312,312),inner(-58,-58,116,116);
                    path.arcMoveTo(outer,90+step/2-gap-slot*step);path.arcTo(outer,90+step/2-gap-slot*step,-step+2*gap);
                    path.arcTo(inner,90-step/2+gap-slot*step,step-2*gap);path.closeSubpath();
                }
            }
        }
        return result;
    }();
    Q_ASSERT(supportedSlotCount(count) && index>=0 && index<count && shape>=WheelShape::Original && shape<=WheelShape::Capsule);
    return paths[int(shape)][count/4-1][index];
}
int Geometry::hit(QPointF position,WheelShape shape,int count) const {
    if(radius<=0) return -1;
    const auto point=(position-center)*(extent/radius);
    for(int index=0;index<count;++index) if(slotPath(shape,index,count).contains(point)) return index;
    return -1;
}
}
