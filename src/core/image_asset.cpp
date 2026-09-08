#include <QCoreApplication>
#include "core/image_asset.h"
#include <QImageReader>
#include <QBuffer>
#include <QFileInfo>
#include <QFile>
namespace wheel {
QImage decodeImageAsset(const QByteArray& png) {
    if(png.isEmpty() || png.size()>65536) return {};
    QBuffer buffer; buffer.setData(png); buffer.open(QIODevice::ReadOnly);
    QImageReader reader(&buffer,"PNG");
    const auto size=reader.size();
    if(!size.isValid() || size.width()>128 || size.height()>128) return {};
    return reader.read();
}
bool importImageAsset(const QString& path,QByteArray& png,QString& error) {
    QFile file(path);
    if(!file.open(QIODevice::ReadOnly) || file.size()>10*1024*1024) {error=QCoreApplication::translate("MouseWheel","无法读取图片或文件过大。"); return false;}
    return importImageAsset(file.readAll(),png,error);
}
bool importImageAsset(const QByteArray& bytes,QByteArray& png,QString& error) {
    error.clear(); QBuffer input; input.setData(bytes); input.open(QIODevice::ReadOnly);
    QImageReader reader(&input); reader.setAutoTransform(true); reader.setDecideFormatFromContent(true);
    const auto size=reader.size();
    if(bytes.size()>10*1024*1024 || !size.isValid() || qint64(size.width())*size.height()>20000000) {
        error=QCoreApplication::translate("MouseWheel","请选择小于 10 MB、2000 万像素的图片。"); return false;
    }
    reader.setScaledSize(size.scaled(96,96,Qt::KeepAspectRatio));
    const auto image=reader.read();
    if(image.isNull()) { error=QCoreApplication::translate("MouseWheel","无法读取图片。"); return false; }
    QByteArray data; QBuffer buffer(&data); buffer.open(QIODevice::WriteOnly);
    if(!image.scaled(96,96,Qt::KeepAspectRatio,Qt::SmoothTransformation).save(&buffer,"PNG")) {
        error=QCoreApplication::translate("MouseWheel","无法保存图片。"); return false;
    }
    png=std::move(data); return true;
}
}
