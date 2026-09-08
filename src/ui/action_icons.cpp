#include "ui/action_icons.h"
#include "ui/library_icon.h"
#include "core/image_asset.h"
#include <QSvgRenderer>
#include <QFile>
#include "platform/program_icon.h"
#include <QPainter>
#include <QPixmapCache>
#include <QFileInfo>
#include <QDir>
#include <QDateTime>
int qInitResources_icons();
namespace wheel {
QIcon symbolIcon(const QString& name,const QColor& color) {
    static const bool initialized=::qInitResources_icons()!=0; Q_UNUSED(initialized);
    QFile file(":/icons/"+name+".svg"); if(!file.open(QIODevice::ReadOnly)) return {};
    auto svg=file.readAll(); svg.replace("currentColor",color.name().toUtf8());
    QSvgRenderer renderer(svg); QPixmap image(96,96); image.fill(Qt::transparent);
    QPainter painter(&image); renderer.render(&painter); return QIcon(image);
}
QIcon actionIcon(const Slot& slot,const QColor& color,const QString& assetDirectory) {
    if(!slot.enabled()) return symbolIcon("plus",color);
    switch(slot.icon.source) {
    case IconSource::Library: {
        const QIcon icon=assetDirectory.isEmpty()?QIcon{}:libraryIcon(QDir(assetDirectory).filePath(slot.icon.value));
        return icon.isNull()?symbolIcon("app-window",color):icon;
    }
    case IconSource::Builtin: return symbolIcon(slot.icon.value,color);
    case IconSource::Program: {
        const QFileInfo file(slot.icon.value);
        const auto key="program:"+file.absoluteFilePath()+":"+QString::number(file.lastModified().toMSecsSinceEpoch());
        QPixmap image;
        if(!QPixmapCache::find(key,&image)) {
            image=QPixmap::fromImage(win::programIcon(slot.icon.value));
            if(!image.isNull()) QPixmapCache::insert(key,image);
        }
        const QIcon icon(image);
        return icon.isNull()?symbolIcon("app-window",color):icon;
    }
    case IconSource::Automatic: {
        if(const auto* website=std::get_if<WebsiteAction>(&slot.action)) {
            if(slot.icon.value==website->url && !slot.icon.image.isEmpty()) return QIcon(QPixmap::fromImage(decodeImageAsset(slot.icon.image)));
            return symbolIcon("globe",color);
        }
        Slot automatic=slot; automatic.icon=suggestedIcon(slot.action); return actionIcon(automatic,color,assetDirectory);
    }
    case IconSource::Image: return QIcon(QPixmap::fromImage(decodeImageAsset(slot.icon.image)));
    }
    return {};
}
}
