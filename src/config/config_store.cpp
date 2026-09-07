#include "config/config_store.h"
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSaveFile>
namespace wheel {
ConfigStore::ConfigStore(QString path, QObject* parent) : QObject(parent), path_(std::move(path)) {}
bool ConfigStore::load() {
    error_.clear();
    if (!QFile::exists(path_)) { blocked_ = false; return true; }
    QFile file(path_);
    if (!file.open(QIODevice::ReadOnly)) {
        error_ = file.errorString(); blocked_ = true; return false;
    }
    if (file.size() > 65536) { error_ = QStringLiteral("配置文件过大。"); blocked_ = true; return false; }
    QJsonParseError parse;
    const auto doc = QJsonDocument::fromJson(file.readAll(), &parse);
    const auto obj = doc.object();
    const auto slots = obj["slots"].toArray();
    Config candidate;
    candidate.modifier = static_cast<Modifier>(obj["modifier"].toInt(-1));
    candidate.button = static_cast<MouseButton>(obj["button"].toInt(-1));
    candidate.theme = static_cast<Theme>(obj["theme"].toInt(-1));
    bool valid = doc.isObject() && parse.error == QJsonParseError::NoError &&
                 obj["version"].toInt(-1) == 1 && slots.size() == 8;
    if (valid) {
        for (int i=0; i<8; ++i) {
            auto value = slots[i].toObject();
            if (!value["name"].isString() || !value["key"].isDouble() ||
                !value["modifiers"].isDouble()) { valid = false; break; }
            candidate.slots[i] = {value["name"].toString(),
                {value["key"].toInt(-1), static_cast<unsigned>(value["modifiers"].toInt(-1))}};
        }
    }
    if (!valid || !validate(candidate).isEmpty()) {
        error_ = QStringLiteral("配置损坏或版本不受支持。原文件已保留；请明确重置后使用。");
        blocked_ = true; return false;
    }
    blocked_ = false; current_ = candidate;
    return true;
}
bool ConfigStore::commit(const Config& config) {
    if (blocked_) { error_ = QStringLiteral("请先重置不可用的配置。"); return false; }
    error_ = validate(config);
    if (!error_.isEmpty()) return false;
    QJsonArray slots;
    for (const auto& slot : config.slots)
        slots.append(QJsonObject{{"name",slot.name}, {"key",slot.shortcut.key},
                                 {"modifiers",static_cast<int>(slot.shortcut.modifiers)}});
    QJsonObject obj{{"version",1}, {"modifier",static_cast<int>(config.modifier)},
                    {"button",static_cast<int>(config.button)}, {"theme",static_cast<int>(config.theme)},
                    {"slots",slots}};
    QSaveFile file(path_);
    file.setDirectWriteFallback(false);
    const auto data = QJsonDocument(obj).toJson();
    if (!file.open(QIODevice::WriteOnly) || file.write(data) != data.size() || !file.commit()) {
        error_ = file.errorString(); return false;
    }
    current_ = config; Q_EMIT changed(current_); return true;
}
bool ConfigStore::reset() {
    const bool wasBlocked = blocked_;
    blocked_ = false;
    if (commit(defaultConfig())) return true;
    blocked_ = wasBlocked; return false;
}
}
