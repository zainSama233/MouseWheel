#pragma once
#include <QWidget>
#include "core/model.h"
namespace wheel {
class WheelWindow final : public QWidget {
    Q_OBJECT
public:
    explicit WheelWindow(bool overlay = true, QWidget* parent = nullptr);
    void present(quint64 session, Config config, Geometry geometry, const QString& screen);
    void select(quint64 session, int index);
    void dismiss(quint64 session);
    void preview(const Config& config);
Q_SIGNALS:
    void hidden(quint64 session);
    void firstPaint(quint64 session, qint64 nanoseconds);
protected:
    void paintEvent(QPaintEvent*) override;
    bool nativeEvent(const QByteArray&, void*, qintptr*) override;
private:
    bool overlay_;
    bool painted_ = false;
    quint64 session_ = 0;
    int selection_ = -1;
    Config config_ = defaultConfig();
};
}
