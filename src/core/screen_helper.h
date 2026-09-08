#pragma once
#include "core/model.h"
#include <QFont>
namespace wheel {
struct SlotContent {
    QRectF icon,text;
    QFont font;
    QString label;
    QRectF bounds() const {return label.isEmpty()?icon:icon.united(text);}
};
class ScreenHelper final {
public:
    static Geometry fit(QPointF cursor,QRectF available,double scale,double extent=WheelRadius,QPointF margin={},EdgePolicy policy=EdgePolicy::Translate);
    static QPointF toLocal(QPointF point,const Geometry& geometry);
    static QPointF toNative(QPointF point,const Geometry& geometry);
    static SlotContent content(const Slot& slot,const SlotStyle& style,QPointF center);
    static double extent(const WheelConfig& wheel);
};
}
