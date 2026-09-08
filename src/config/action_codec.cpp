#include "config/action_codec.h"
#include <QJsonArray>
#include "config/workspace_codec.h"
namespace wheel {
QJsonObject encodeSlot(const Slot& slot) {
    QJsonObject data;
    std::visit([&](const auto& a) {
        using T=std::decay_t<decltype(a)>;
        if constexpr(std::is_same_v<T,GroupAction>) { QJsonArray children;for(const auto& child:a.slots) children.append(encodeSlot(child));data={{"slots",children}}; }
        else if constexpr(std::is_same_v<T,Shortcut>) data={{"key",a.key},{"modifiers",int(a.modifiers)}};
        else if constexpr(std::is_same_v<T,ApplicationAction>) data={{"path",a.path},{"arguments",a.arguments},{"directory",a.directory},{"normalUser",a.normalUser}};
        else if constexpr(std::is_same_v<T,WebsiteAction>) data={{"url",a.url},{"browser",int(a.browser)},{"executable",a.executable}};
        else if constexpr(std::is_same_v<T,FolderAction>) data={{"location",int(a.location)},{"path",a.path}};
        else if constexpr(std::is_same_v<T,CommandAction>) data={{"shell",int(a.shell)},{"script",a.script},{"directory",a.directory},{"hidden",a.hidden}};
        else if constexpr(std::is_same_v<T,OcrAction>) data={{"provider",int(a.provider)},{"endpoint",a.endpoint},{"apiKey",a.apiKey},{"model",a.model},{"resultPath",a.resultPath}};
        else if constexpr(std::is_same_v<T,WindowAction>) data={{"operation",int(a.operation)},{"opacity",a.opacity}};
        else if constexpr(std::is_same_v<T,SystemAction>) data={{"operation",int(a.operation)}};
    },slot.action);
    QJsonObject result{{"name",slot.name},{"kind",int(slot.kind())},{"action",data},{"showLabel",slot.showLabel},
        {"icon",QJsonObject{{"source",int(slot.icon.source)},{"value",slot.icon.value},{"image",QString::fromLatin1(slot.icon.image.toBase64())}}}};
    if(slot.style!=SlotStyle{}) result["style"]=encodeStyle(slot.style);
    return result;
}
std::optional<Slot> decodeSlot(const QJsonObject& obj,bool allowGroup) {
    Slot slot;
    if(obj.contains("style")) {if(!obj["style"].isObject())return {};auto style=decodeStyle(obj["style"].toObject());if(!style)return {};slot.style=*style;}
    slot.name=obj["name"].toString(); const auto a=obj["action"].toObject();
    switch(obj["kind"].toInt(-1)) {
    case 0: slot.action=Shortcut{a["key"].toInt(-1),unsigned(a["modifiers"].toInt(-1))}; break;
    case 1: slot.action=ScreenshotAction{}; break;
    case 2: slot.action=AnnotationAction{}; break;
    case 3: slot.action=ApplicationAction{a["path"].toString(),a["arguments"].toString(),a["directory"].toString(),a["normalUser"].toBool()}; break;
    case 4: slot.action=WebsiteAction{a["url"].toString(),Browser(a["browser"].toInt(-1)),a["executable"].toString()}; break;
    case 5: slot.action=FolderAction{FolderLocation(a["location"].toInt(-1)),a["path"].toString()}; break;
    case 6: slot.action=CommandAction{Shell(a["shell"].toInt(-1)),a["script"].toString(),a["directory"].toString(),a["hidden"].toBool()}; break;
    case 7: slot.action=OcrAction{OcrProvider(a["provider"].toInt(-1)),a["endpoint"].toString(),a["apiKey"].toString(),a["model"].toString(),a["resultPath"].toString()}; break;
    case 8: slot.action=WindowAction{WindowOperation(a["operation"].toInt(-1)),a["opacity"].toInt(-1)}; break;
    case 9: slot.action=SystemAction{SystemOperation(a["operation"].toInt(-1))}; break;
    case 10: {
        if(!allowGroup || !a["slots"].isArray() || !supportedSlotCount(a["slots"].toArray().size())) return {};
        GroupAction group;group.slots.clear();
        for(const auto& value:a["slots"].toArray()) {const auto child=decodeSlot(value.toObject(),false);if(!child) return {};group.slots.append(*child);}
        slot.action=std::move(group);break;
    }
    default: return {};
    }
    const auto icon=obj["icon"].toObject();
    slot.icon={IconSource(icon["source"].toInt(-1)),icon["value"].toString(),QByteArray::fromBase64(icon["image"].toString().toLatin1(),QByteArray::AbortOnBase64DecodingErrors)};
    slot.showLabel=obj["showLabel"].toBool();
    // Canonical comparison rejects missing fields, wrong types and lossy number conversions.
    if(encodeSlot(slot)!=obj || !validate(slot).isEmpty()) return {};
    return slot;
}
}
