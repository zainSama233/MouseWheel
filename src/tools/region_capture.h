#pragma once
#include <QObject>
#include <QImage>
#include "core/model.h"
class QScreen;
namespace wheel {
class RegionPicker;
class RegionCapture final:public QObject {
    Q_OBJECT
public:
    explicit RegionCapture(QObject* parent=nullptr);
    ~RegionCapture() override;
    bool active() const;
    bool start(Theme theme,QString& error);
    void cancel();
Q_SIGNALS:
    void activeChanged();
    void selected(QImage image,QScreen* screen);
private:
    void clearPickers();
    quint64 generation_=0;
    QList<RegionPicker*> pickers_;
};
}
