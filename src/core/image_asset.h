#pragma once
#include <QImage>
#include <QString>
namespace wheel {
QImage decodeImageAsset(const QByteArray& png);
bool importImageAsset(const QString& path,QByteArray& png,QString& error);
}
