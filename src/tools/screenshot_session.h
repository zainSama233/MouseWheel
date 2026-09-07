#pragma once
#include <QObject>
#include <QPointer>
#include <QImage>
#include "core/model.h"
namespace wheel {
class PinnedImage;
class RegionPicker;
class ScreenshotSession final : public QObject {
    Q_OBJECT
public:
    explicit ScreenshotSession(QObject* parent=nullptr);
    ~ScreenshotSession() override;
    bool active() const;
    void cancel();
    bool start(Theme theme,QString& error);
Q_SIGNALS:
    void activeChanged();
private:
    void clearPickers();
    quint64 generation_=0;
    QList<RegionPicker*> pickers_;
    QList<PinnedImage*> pins_;
};
}
