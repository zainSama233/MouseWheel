#pragma once
#include <QObject>
#include <memory>
#include "core/model.h"
namespace wheel {
class ShortcutCapture final:public QObject {
    Q_OBJECT
public:
    explicit ShortcutCapture(QObject* owner);
    ~ShortcutCapture() override;
    bool start();
    void cancel();
    static ShortcutCapture* active();
Q_SIGNALS:
    void recorded(wheel::Shortcut shortcut);
    void stopped();
private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
    void finish();
};
}
