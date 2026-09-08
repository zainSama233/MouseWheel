#pragma once
#include <QImage>
#include <QString>
namespace wheel {
QImage decodeCenterImage(const QByteArray& png);
bool importCenterImage(const QString& path,QByteArray& png,QString& error);
}
