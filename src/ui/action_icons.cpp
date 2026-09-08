#include "ui/action_icons.h"
#include "core/image_asset.h"
#include <QSvgRenderer>
#include <QFile>
#include <QFileIconProvider>
#include <QPainter>
int qInitResources_icons();
namespace wheel {
QIcon symbolIcon(const QString& name,const QColor& color) {
    static const bool initialized=::qInitResources_icons()!=0; Q_UNUSED(initialized);
    QFile file(":/icons/"+name+".svg"); if(!file.open(QIODevice::ReadOnly)) return {};
    auto svg=file.readAll(); svg.replace("currentColor",color.name().toUtf8());
    QSvgRenderer renderer(svg); QPixmap image(96,96); image.fill(Qt::transparent);
    QPainter painter(&image); renderer.render(&painter); return QIcon(image);
}
QIcon actionIcon(const Slot& slot,const QColor& color) {
    if(!slot.enabled()) return symbolIcon("plus",color);
    switch(slot.icon.source) {
    case IconSource::Builtin: return symbolIcon(slot.icon.value,color);
    case IconSource::Program: {
        QFileIconProvider provider; const auto icon=provider.icon(QFileInfo(slot.icon.value));
        return icon.isNull()?symbolIcon("app-window",color):icon;
    }
    case IconSource::Image: return QIcon(QPixmap::fromImage(decodeImageAsset(slot.icon.image)));
    }
    return {};
}
}
