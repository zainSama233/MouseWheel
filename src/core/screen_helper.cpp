#include "core/screen_helper.h"
#include <QFontMetricsF>
#include <algorithm>
namespace wheel {
Geometry ScreenHelper::fit(QPointF cursor,QRectF available,double scale,double extent,QPointF margin,EdgePolicy policy) {
    const double mx=std::min(margin.x()*scale,std::max(0.,available.width()/2-1));
    const double my=std::min(margin.y()*scale,std::max(0.,available.height()/2-1));
    available.adjust(mx,my,-mx,-my);
    double r=std::min({extent*scale,available.width()/2,available.height()/2});
    if(policy==EdgePolicy::Shrink) {
        const double distance=std::min({cursor.x()-available.left(),available.right()-cursor.x(),cursor.y()-available.top(),available.bottom()-cursor.y()});
        r=std::min(r,std::max(r*.45,distance));
    }
    return {{std::clamp(cursor.x(),available.left()+r,available.right()-r),std::clamp(cursor.y(),available.top()+r,available.bottom()-r)},r,extent};
}
QPointF ScreenHelper::toLocal(QPointF point,const Geometry& g) {return (point-g.center)*(g.extent/g.radius);}
QPointF ScreenHelper::toNative(QPointF point,const Geometry& g) {return g.center+point*(g.radius/g.extent);}
SlotContent ScreenHelper::content(const Slot& slot,const SlotStyle& style,QPointF center) {
    SlotContent result;result.font=QFont(style.fontFamily.value_or(QString("Microsoft YaHei UI")));result.font.setPixelSize(style.fontSize.value_or(10));
    const double size=style.iconSize.value_or(30);const auto layout=style.layout.value_or(ContentLayout::Below);
    const QFontMetricsF metrics(result.font);const double height=metrics.height();
    if(slot.showLabel && layout!=ContentLayout::IconOnly)result.label=metrics.elidedText(slot.name,Qt::ElideRight,std::max(48.,size*2));
    const double width=metrics.horizontalAdvance(result.label);center+=QPointF(style.offsetX.value_or(0),style.offsetY.value_or(0));
    result.icon={center.x()-size/2,center.y()-size/2,size,size};
    if(result.label.isEmpty())return result;
    switch(layout) {
    case ContentLayout::Below:result.icon.translate(0,-(height+4)/2);result.text={center.x()-width/2,result.icon.bottom()+4,width,height};break;
    case ContentLayout::Above:result.icon.translate(0,(height+4)/2);result.text={center.x()-width/2,result.icon.top()-height-4,width,height};break;
    case ContentLayout::Left:result.icon.translate((width+4)/2,0);result.text={result.icon.left()-width-4,center.y()-height/2,width,height};break;
    case ContentLayout::Right:result.icon.translate(-(width+4)/2,0);result.text={result.icon.right()+4,center.y()-height/2,width,height};break;
    case ContentLayout::IconOnly:break;
    }
    return result;
}
double ScreenHelper::extent(const WheelConfig& wheel) {
    double extent=WheelRadius;
    const auto include=[&](const QRectF& rect){extent=std::max({extent,std::abs(rect.left()),std::abs(rect.right()),std::abs(rect.top()),std::abs(rect.bottom())});};
    const auto page=[&](const QList<Slot>& slots){for(int i=0;i<slots.size();++i){
        const auto style=cascadeStyle(wheel.style,slots[i].style);const double extra=style.glowRadius.value_or(0)+style.borderWidth.value_or(0)/2;
        include(slotPath(wheel.shape,i,slots.size()).boundingRect().adjusted(-extra,-extra,extra,extra));
        if(slots[i].enabled())include(content(slots[i],style,slotCenter(i,slots.size(),wheel.shape)).bounds());
    }};
    page(wheel.slots);for(const auto& slot:wheel.slots)if(const auto* group=std::get_if<GroupAction>(&slot.action))page(group->slots);
    if(wheel.centerEnabled)include(content(wheel.center,cascadeStyle(wheel.style,wheel.center.style),{}).bounds());
    return extent+2;
}
}
