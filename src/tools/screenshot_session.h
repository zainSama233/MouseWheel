#pragma once
#include <QObject>
#include "tools/region_capture.h"
namespace wheel {
class PinnedImage;
class ScreenshotSession final:public QObject {
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
    RegionCapture capture_;
    Theme theme_=Theme::Light;
    QList<PinnedImage*> pins_;
};
}
