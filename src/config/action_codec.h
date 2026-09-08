#pragma once
#include "core/model.h"
#include <QJsonObject>
namespace wheel {
QJsonObject encodeSlot(const Slot& slot);
std::optional<Slot> decodeSlot(const QJsonObject& object);
}
