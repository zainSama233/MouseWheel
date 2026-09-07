#pragma once
#include <QObject>
#include "core/model.h"
namespace wheel {
class ConfigStore final : public QObject {
    Q_OBJECT
public:
    explicit ConfigStore(QString path, QObject* parent = nullptr);
    bool load();
    bool commit(const Config& config);
    bool reset();
    const Config& current() const { return current_; }
    QString path() const { return path_; }
    QString error() const { return error_; }
    bool blocked() const { return blocked_; }
Q_SIGNALS:
    void changed(wheel::Config config);
private:
    QString path_;
    QString error_;
    Config current_ = defaultConfig();
    bool blocked_ = false;
};
}
