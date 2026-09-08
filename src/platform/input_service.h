#pragma once
#include <QObject>
#include <memory>
#include "core/model.h"
namespace wheel {
class InputService final : public QObject {
    Q_OBJECT
public:
    explicit InputService(QObject* parent = nullptr);
    ~InputService() override;
    void start(Config config);
    void configure(Config config);
    void pause(bool paused);
    void hidden(quint64 session);
    void restart();
Q_SIGNALS:
    void actionRequested(wheel::Slot action);
    void triggered(quint64 session, qint64 nanoseconds);
    void showWheel(quint64 session, wheel::Config config, wheel::Geometry geometry, QString screen);
    void selection(quint64 session, int index);
    void hideWheel(quint64 session);
    void failure(QString error);
    void listening(bool available);
private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};
}
