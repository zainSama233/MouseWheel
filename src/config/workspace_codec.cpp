#include "config/workspace_codec.h"
#include "config/action_codec.h"
#include <QJsonArray>
namespace wheel {
QJsonObject encodeStyle(const SlotStyle& s) {
    QJsonObject o;
    const auto color=[&](const char* key,const std::optional<QColor>& v){if(v)o[key]=v->name(QColor::HexArgb);};
    color("fill",s.fill);color("glow",s.glow);color("border",s.border);color("text",s.text);
    if(s.fontFamily)o["fontFamily"]=*s.fontFamily;
    if(s.fontSize)o["fontSize"]=*s.fontSize;if(s.iconSize)o["iconSize"]=*s.iconSize;
    if(s.offsetX)o["offsetX"]=*s.offsetX;if(s.offsetY)o["offsetY"]=*s.offsetY;
    if(s.borderWidth)o["borderWidth"]=*s.borderWidth;if(s.glowRadius)o["glowRadius"]=*s.glowRadius;
    if(s.layout)o["layout"]=int(*s.layout);return o;
}
std::optional<SlotStyle> decodeStyle(const QJsonObject& o) {
    SlotStyle s;
    const auto color=[&](const char* key,std::optional<QColor>& v){if(o.contains(key))v=QColor(o[key].toString());};
    color("fill",s.fill);color("glow",s.glow);color("border",s.border);color("text",s.text);
    if(o.contains("fontFamily"))s.fontFamily=o["fontFamily"].toString();
    if(o.contains("fontSize"))s.fontSize=o["fontSize"].toInt(-1);if(o.contains("iconSize"))s.iconSize=o["iconSize"].toInt(-1);
    if(o.contains("offsetX"))s.offsetX=o["offsetX"].toDouble();if(o.contains("offsetY"))s.offsetY=o["offsetY"].toDouble();
    if(o.contains("borderWidth"))s.borderWidth=o["borderWidth"].toDouble();if(o.contains("glowRadius"))s.glowRadius=o["glowRadius"].toDouble();
    if(o.contains("layout"))s.layout=ContentLayout(o["layout"].toInt(-1));
    if(encodeStyle(s)!=o || !validate(s).isEmpty()) return {};return s;
}
QJsonObject encodeWheel(const WheelConfig& w) {
    QJsonArray slots;for(const auto& slot:w.slots)slots.append(encodeSlot(slot));
    return {{"theme",int(w.theme)},{"shape",int(w.shape)},{"slots",slots},{"centerImage",QString::fromLatin1(w.centerImage.toBase64())},
        {"center",encodeSlot(w.center)},{"centerEnabled",w.centerEnabled},{"deadZone",w.deadZone},{"style",encodeStyle(w.style)},
        {"frosted",w.frosted},{"marginX",w.safetyMargin.x()},{"marginY",w.safetyMargin.y()},{"edgePolicy",int(w.edgePolicy)}};
}
std::optional<WheelConfig> decodeWheel(const QJsonObject& o) {
    WheelConfig w;w.theme=Theme(o["theme"].toInt(-1));w.shape=WheelShape(o["shape"].toInt(-1));
    const auto slots=o["slots"].toArray();if(!supportedSlotCount(slots.size()))return {};w.slots.clear();
    for(const auto& value:slots){auto slot=decodeSlot(value.toObject());if(!slot)return {};w.slots.append(*slot);}
    auto center=decodeSlot(o["center"].toObject(),false);auto style=decodeStyle(o["style"].toObject());if(!center || !style)return {};
    w.center=*center;w.style=*style;w.centerEnabled=o["centerEnabled"].toBool();w.deadZone=o["deadZone"].toDouble();
    w.centerImage=QByteArray::fromBase64(o["centerImage"].toString().toLatin1(),QByteArray::AbortOnBase64DecodingErrors);
    w.frosted=o["frosted"].toBool();w.safetyMargin={o["marginX"].toDouble(),o["marginY"].toDouble()};w.edgePolicy=EdgePolicy(o["edgePolicy"].toInt(-1));
    if(encodeWheel(w)!=o || !validate(w).isEmpty())return {};return w;
}
QJsonObject encodeWorkspace(const Config& c) {
    QJsonArray profiles,assets,colors;
    for(const auto& p:c.profiles)profiles.append(QJsonObject{{"id",p.id},{"name",p.name},{"applications",QJsonArray::fromStringList(p.applications)},{"wheel",encodeWheel(p.wheel)}});
    for(const auto& a:c.assets)assets.append(QJsonObject{{"id",a.id},{"name",a.name}});
    for(const auto& p:c.colors)colors.append(QJsonObject{{"id",p.id},{"name",p.name},{"color",p.color.name(QColor::HexArgb)}});
    return {{"version",4},{"modifier",int(c.modifier)},{"button",int(c.button)},{"language",int(c.language)},
        {"global",encodeWheel(c)},{"profiles",profiles},{"assets",assets},{"colors",colors},
        {"triggerRules",QJsonObject{{"pauseFullscreen",c.triggerRules.pauseFullscreen},{"excludedApplications",QJsonArray::fromStringList(c.triggerRules.excludedApplications)}}}};
}
std::optional<Config> decodeWorkspace(const QJsonObject& o) {
    Config c;auto global=decodeWheel(o["global"].toObject());if(!global)return {};static_cast<WheelConfig&>(c)=*global;
    c.modifier=Modifier(o["modifier"].toInt(-1));c.button=MouseButton(o["button"].toInt(-1));c.language=Language(o["language"].toInt(-1));
    const auto rules=o["triggerRules"].toObject();c.triggerRules.pauseFullscreen=rules["pauseFullscreen"].toBool();
    for(const auto& path:rules["excludedApplications"].toArray())c.triggerRules.excludedApplications.append(path.toString());
    for(const auto& value:o["profiles"].toArray()) {const auto p=value.toObject();auto wheel=decodeWheel(p["wheel"].toObject());if(!wheel)return {};
        Profile profile;profile.id=p["id"].toString();profile.name=p["name"].toString();profile.wheel=*wheel;
        for(const auto& path:p["applications"].toArray())profile.applications.append(path.toString());c.profiles.append(profile);
    }
    for(const auto& value:o["assets"].toArray()){const auto a=value.toObject();c.assets.append({a["id"].toString(),a["name"].toString()});}
    for(const auto& value:o["colors"].toArray()){const auto a=value.toObject();c.colors.append({a["id"].toString(),a["name"].toString(),QColor(a["color"].toString())});}
    if(encodeWorkspace(c)!=o || !validate(c).isEmpty())return {};return c;
}
}
