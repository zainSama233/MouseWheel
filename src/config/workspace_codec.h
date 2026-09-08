#pragma once
#include "core/model.h"
#include <QJsonObject>
namespace wheel {
QJsonObject encodeStyle(const SlotStyle& style);
std::optional<SlotStyle> decodeStyle(const QJsonObject& object);
QJsonObject encodeWheel(const WheelConfig& wheel);
std::optional<WheelConfig> decodeWheel(const QJsonObject& object);
QJsonObject encodeWorkspace(const Config& config);
std::optional<Config> decodeWorkspace(const QJsonObject& object);
}
