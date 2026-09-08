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
    if (file.size() > 262144) { error_ = QStringLiteral("配置文件过大。"); blocked_ = true; return false; }
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
    if(obj.contains("shape") && (!obj["shape"].isDouble() || obj["shape"].toDouble()!=obj["shape"].toInt(-1))) valid=false;
    candidate.shape=static_cast<WheelShape>(obj["shape"].toInt(static_cast<int>(WheelShape::Circle)));
    if(obj.contains("centerImage")) {
        if(!obj["centerImage"].isString()) valid=false;
        const auto encoded=obj["centerImage"].toString().toLatin1();
        candidate.centerImage=QByteArray::fromBase64(encoded,QByteArray::AbortOnBase64DecodingErrors);
        if(!encoded.isEmpty() && candidate.centerImage.isEmpty()) valid=false;
    }
    if (valid) {
        for (int i=0; i<8; ++i) {
            auto value = slots[i].toObject();
            if (!value["name"].isString() || !value["key"].isDouble() ||
                !value["modifiers"].isDouble()) { valid = false; break; }
            candidate.slots[i] = {value["name"].toString(),
                {value["key"].toInt(-1), static_cast<unsigned>(value["modifiers"].toInt(-1))},
                static_cast<ActionKind>(value["kind"].toInt(0)),value["target"].toString()};
            if(value.contains("target") && !value["target"].isString()) { valid=false; break; }
            if (value.contains("kind") && (!value["kind"].isDouble() || value["kind"].toDouble()!=value["kind"].toInt(-1))) { valid=false; break; }
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
        slots.append(QJsonObject{{"name",slot.name}, {"target",slot.target}, {"kind",static_cast<int>(slot.kind)}, {"key",slot.shortcut.key},
                                 {"modifiers",static_cast<int>(slot.shortcut.modifiers)}});
    QJsonObject obj{{"version",1}, {"modifier",static_cast<int>(config.modifier)},
                    {"button",static_cast<int>(config.button)}, {"theme",static_cast<int>(config.theme)},
                    {"slots",slots}, {"shape",static_cast<int>(config.shape)}, {"centerImage",QString::fromLatin1(config.centerImage.toBase64())}};
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
