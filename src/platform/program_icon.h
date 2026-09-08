#pragma once
#include <QImage>
#include <QString>
namespace wheel::win {
struct ShortcutInfo { QString target,iconFile; int iconIndex=0; };
ShortcutInfo readShortcut(const QString& path);
QImage programIcon(const QString& path);
}
