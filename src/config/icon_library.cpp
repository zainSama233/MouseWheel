#include <QCoreApplication>
#include "config/config_store.h"
#include "core/image_asset.h"
#include <QFile>
#include <QFileInfo>
#include <QDir>
#include <QSaveFile>
#include <QCryptographicHash>
#include <QRegularExpression>
#include <QImageReader>
#include <QBuffer>
#include <QXmlStreamReader>
namespace wheel {
QString ConfigStore::assetDirectory() const {return QFileInfo(path_).absoluteDir().filePath("icons");}
QString ConfigStore::assetPath(const QString& id) const {
    static const QRegularExpression valid("^[a-f0-9]{64}\\.(svg|png|ico|jpg)$");
    return valid.match(id).hasMatch()?QDir(assetDirectory()).filePath(id):QString{};
}
QString ConfigStore::importIcon(const QString& source) {
    error_.clear();QFile input(source);if(!input.open(QIODevice::ReadOnly) || input.size()>10*1024*1024) {error_=QCoreApplication::translate("MouseWheel","无法读取图标，文件需小于 10 MB。");return {};}
    const auto bytes=input.readAll();QByteArray normalized;
    if(!importImageAsset(bytes,normalized,error_))return {};
    QBuffer buffer;buffer.setData(bytes);buffer.open(QIODevice::ReadOnly);QImageReader reader(&buffer);reader.setDecideFormatFromContent(true);
    auto format=reader.format();if(format=="jpeg")format="jpg";
    if(format!="svg" && format!="png" && format!="ico" && format!="jpg") {error_=QCoreApplication::translate("MouseWheel","请选择 SVG、PNG、ICO 或 JPG 图标。");return {};}
    if(format=="svg") {
        QXmlStreamReader xml(bytes);
        while(!xml.atEnd()) {xml.readNext();if(!xml.isStartElement())continue;
            for(const auto& attribute:xml.attributes()) if(attribute.name()=="href" && !attribute.value().startsWith('#') && !attribute.value().startsWith(u"data:")) {error_=QCoreApplication::translate("MouseWheel","SVG 包含外部图片，请先将图片嵌入 SVG。");return {};}
        }
        if(xml.hasError()) {error_=QCoreApplication::translate("MouseWheel","SVG 文件无效。");return {};}
    }
    auto draft=current_;QStringList created;const auto id=storeIcon(bytes,format,QFileInfo(source).completeBaseName(),draft,created);
    if(id.isEmpty() || !commit(draft)){for(const auto& path:created)QFile::remove(path);return {};}
    return id;
}
QString ConfigStore::storeIcon(const QByteArray& bytes,const QByteArray& format,const QString& name,Config& draft,QStringList& created) {
    const auto id=QString::fromLatin1(QCryptographicHash::hash(bytes,QCryptographicHash::Sha256).toHex()+'.'+format);
    if(blocked_ || !QFileInfo(path_).absoluteDir().exists() || !QDir().mkpath(assetDirectory())) {error_=QCoreApplication::translate("MouseWheel","配置目录不可写。");return {};}
    if(!QFile::exists(assetPath(id))) {
        QSaveFile file(assetPath(id));if(!file.open(QIODevice::WriteOnly) || file.write(bytes)!=bytes.size() || !file.commit()){error_=file.errorString();return {};}
        created.append(assetPath(id));
    }
    if(std::none_of(draft.assets.begin(),draft.assets.end(),[&](const auto& asset){return asset.id==id;}))draft.assets.append({id,name.trimmed().isEmpty()?QString("Icon"):name.left(64)});
    return id;
}
bool ConfigStore::migrateIcons() {
    auto draft=current_;QStringList created;bool ok=true;
    visitSlots(draft,[&](Slot& slot){if(!ok || slot.icon.source!=IconSource::Image)return;
        const auto id=storeIcon(slot.icon.image,"png",slot.name,draft,created);if(id.isEmpty()){ok=false;return;}slot.icon={IconSource::Library,id,{}};
    });
    const auto center=[&](WheelConfig& wheel){if(!ok || wheel.centerImage.isEmpty())return;
        const auto id=storeIcon(wheel.centerImage,"png","Center",draft,created);if(id.isEmpty()){ok=false;return;}wheel.center.icon={IconSource::Library,id,{}};wheel.centerImage.clear();
    };
    center(draft);for(auto& profile:draft.profiles)center(profile.wheel);
    if(ok && (draft==current_ || commit(draft)))return true;
    for(const auto& path:created)QFile::remove(path);return false;
}
int ConfigStore::iconReferences(const QString& id) const {
    auto config=current_;int count=0;visitSlots(config,[&](Slot& slot){if(slot.icon.source==IconSource::Library && slot.icon.value==id)++count;});return count;
}
bool ConfigStore::renameIcon(const QString& id,const QString& name) {
    auto draft=current_;for(auto& asset:draft.assets)if(asset.id==id) {asset.name=name.trimmed();return commit(draft);}
    error_=QCoreApplication::translate("MouseWheel","图标不存在。");return false;
}
bool ConfigStore::removeIcon(const QString& id,bool replaceReferences) {
    auto draft=current_;const auto found=std::find_if(draft.assets.begin(),draft.assets.end(),[&](const auto& a){return a.id==id;});
    if(found==draft.assets.end()) {error_=QCoreApplication::translate("MouseWheel","图标不存在。");return false;}
    if(iconReferences(id) && !replaceReferences) {error_=QCoreApplication::translate("MouseWheel","图标仍被使用。");return false;}
    visitSlots(draft,[&](Slot& slot){if(slot.icon.source==IconSource::Library && slot.icon.value==id)slot.icon={IconSource::Automatic,{},{}};});
    draft.assets.erase(found);if(!commit(draft))return false;
    if(QFile::exists(assetPath(id)) && !QFile::remove(assetPath(id))) {error_=QCoreApplication::translate("MouseWheel","引用已移除，但图标文件暂时无法删除。");return false;}return true;
}
}
