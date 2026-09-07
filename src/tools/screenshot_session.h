#pragma once
#include <QObject>
#include <QPointer>
#include <QImage>
#include "core/model.h"
namespace wheel {
class ImageEditor;
class RegionPicker;
class ScreenshotSession final : public QObject {
    Q_OBJECT
public:
    explicit ScreenshotSession(QObject* parent=nullptr);
    ~ScreenshotSession() override;
    bool active() const;
    bool start(Theme theme,QString& error);
Q_SIGNALS:
    void activeChanged();
private:
    void clearPickers();
    QList<RegionPicker*> pickers_;
    QPointer<ImageEditor> editor_;
};
}
