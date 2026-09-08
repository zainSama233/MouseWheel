#pragma once
#include <QIcon>
#include <QColor>
#include "core/model.h"
namespace wheel {
QIcon symbolIcon(const QString& name,const QColor& color);
QIcon actionIcon(const Slot& slot,const QColor& color);
}
