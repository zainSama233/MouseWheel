#include "ui/wheel_window.h"
#include "ui/theme.h"
#include "ui/action_icons.h"
#include "core/image_asset.h"
#include "core/clock.h"
#include <QPainter>
#include <QPainterPath>
#include <QGuiApplication>
#include <QScreen>
#include <QMouseEvent>
#include <QApplication>
#include <Windows.h>
namespace wheel {
WheelWindow::WheelWindow(bool overlay, QWidget* parent) : QWidget(parent), opening_(this), overlay_(overlay) {
    opening_.setDuration(120); opening_.setStartValue(0.15); opening_.setEndValue(1.0);
    opening_.setEasingCurve(QEasingCurve::OutCubic);
    connect(&opening_,&QVariantAnimation::valueChanged,this,[this](const QVariant& value){opacity_=value.toDouble(); update();});
    if (overlay_) {
        setWindowFlags(Qt::Tool | Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint |
                       Qt::WindowDoesNotAcceptFocus | Qt::WindowTransparentForInput);
        setAttribute(Qt::WA_TranslucentBackground);
        setAttribute(Qt::WA_ShowWithoutActivating);
        setFocusPolicy(Qt::NoFocus);
        const HWND hwnd = reinterpret_cast<HWND>(winId());
        SetWindowLongPtrW(hwnd,GWL_EXSTYLE,GetWindowLongPtrW(hwnd,GWL_EXSTYLE) |
                         WS_EX_NOACTIVATE | WS_EX_TOOLWINDOW | WS_EX_TRANSPARENT);
    }
    resize(int(WheelRadius*2),int(WheelRadius*2)); applyConfig(config_);
}
void WheelWindow::present(quint64 session, Config config, Geometry geometry, const QString& name) {
    if (session <= session_) return;
    painted_ = false; session_ = session; applyConfig(config); selection_ = -1;
    opening_.stop(); opacity_=0.15;
    for (auto* screen : QGuiApplication::screens()) {
        if (screen->name() != name) continue;
        setScreen(screen);
        resize(qRound(geometry.radius*2/screen->devicePixelRatio()),
               qRound(geometry.radius*2/screen->devicePixelRatio()));
        break;
    }
    const HWND hwnd = reinterpret_cast<HWND>(winId());
    SetWindowPos(hwnd,HWND_TOPMOST,qRound(geometry.center.x()-geometry.radius),
                 qRound(geometry.center.y()-geometry.radius),qRound(geometry.radius*2),
                 qRound(geometry.radius*2),SWP_NOACTIVATE);
    show();
    SetWindowPos(hwnd,HWND_TOPMOST,qRound(geometry.center.x()-geometry.radius),
                 qRound(geometry.center.y()-geometry.radius),qRound(geometry.radius*2),
                 qRound(geometry.radius*2),SWP_NOACTIVATE | SWP_SHOWWINDOW);
    opening_.start(); update();
}
void WheelWindow::select(quint64 session, int index) {
    if (session != session_ || selection_ == index) return;
    selection_ = index; update();
}
void WheelWindow::dismiss(quint64 session) {
    if (session >= session_) { session_ = session; opening_.stop(); hide(); }
    Q_EMIT hidden(session);
}
void WheelWindow::applyConfig(const Config& config) {
    if(cached_ && config_==config) return;
    config_=config; const auto colors=themeColors(config.theme);
    for(int i=0;i<8;++i) {
        icons_[i]=actionIcon(config.slots[i],config.slots[i].enabled()?colors.text:colors.muted);
        selectedIcons_[i]=actionIcon(config.slots[i],colors.selectedText);
    }
    cancelIcon_=symbolIcon("x",colors.muted);
    centerImage_=QPixmap::fromImage(decodeImageAsset(config.centerImage)); cached_=true;
}
void WheelWindow::preview(const Config& config) { applyConfig(config); update(); }
int WheelWindow::previewSlotAt(QPointF position) const {
    const QPointF local((position.x()/width()-.5)*WheelRadius*2,(position.y()/height()-.5)*WheelRadius*2);
    return Geometry{{0,0}}.hit(local,config_.shape);
}
void WheelWindow::mousePressEvent(QMouseEvent* event) {
    if(overlay_ || event->button()!=Qt::LeftButton) return;
    dragStart_=event->position();
    dragSource_=previewSlotAt(dragStart_);
    if(dragSource_>=0) Q_EMIT slotClicked(dragSource_);
}
void WheelWindow::mouseMoveEvent(QMouseEvent* event) {
    if(overlay_ || dragSource_<0 || !(event->buttons()&Qt::LeftButton)) return;
    if((event->position()-dragStart_).manhattanLength()>=QApplication::startDragDistance()) {
        setCursor(Qt::ClosedHandCursor); select(0,previewSlotAt(event->position()));
    }
}
void WheelWindow::mouseReleaseEvent(QMouseEvent* event) {
    if(overlay_ || event->button()!=Qt::LeftButton) return;
    const int source=std::exchange(dragSource_,-1); unsetCursor(); if(source>=0) select(0,source);
    if(source<0 || (event->position()-dragStart_).manhattanLength()<QApplication::startDragDistance()) return;
    const int target=previewSlotAt(event->position());
    if(target>=0 && target!=source) Q_EMIT slotsSwapped(source,target);
}
bool WheelWindow::nativeEvent(const QByteArray& type, void* message, qintptr* result) {
    const auto* msg = static_cast<MSG*>(message);
    if (overlay_ && msg->message == WM_MOUSEACTIVATE) { *result = MA_NOACTIVATE; return true; }
    if (overlay_ && msg->message == WM_NCHITTEST) { *result = HTTRANSPARENT; return true; }
    return QWidget::nativeEvent(type,message,result);
}
void WheelWindow::paintEvent(QPaintEvent*) {
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);
    const auto colors = themeColors(config_.theme);
    p.translate(width()/2.0,height()/2.0);
    p.scale(width()/(WheelRadius*2),height()/(WheelRadius*2));
    p.setOpacity(opacity_);
    const double scale=0.94+0.06*opacity_; p.scale(scale,scale);
    p.setPen(Qt::NoPen);
    for(int i=0;i<8;++i) {
        const bool selected=i==selection_ && (!overlay_ || config_.slots[i].enabled());
        const auto& path=slotPath(config_.shape,i);
        p.fillPath(path,selected?colors.selected:colors.surface);
        const auto center=slotCenter(i);
        const auto& slot=config_.slots[i];
        const bool label=slot.enabled() && slot.showLabel;
        const QRect iconArea(qRound(center.x()-15),qRound(center.y()-(label?24:15)),30,30);
        (selected?selectedIcons_[i]:icons_[i]).paint(&p,iconArea);
        if(label) {
            auto font=p.font(); font.setPixelSize(10); p.setFont(font);
            p.setPen(selected?colors.selectedText:colors.muted);
            const auto text=p.fontMetrics().elidedText(slot.name,Qt::ElideRight,60);
            p.drawText(QRectF(center.x()-30,center.y()+10,60,16),Qt::AlignCenter,text);
            p.setPen(Qt::NoPen);
        }
    }
    const QRectF inner(-CenterRadius,-CenterRadius,CenterRadius*2,CenterRadius*2);
    p.setBrush(colors.surface); p.drawEllipse(inner);
    if(centerImage_.isNull()) cancelIcon_.paint(&p,QRect(-12,-12,24,24));
    else {
        QPainterPath clip; clip.addEllipse(inner.adjusted(4,4,-4,-4)); p.setClipPath(clip);
        const int diameter=qRound((CenterRadius-4)*2);
        auto image=centerImage_.scaled(diameter,diameter,Qt::KeepAspectRatioByExpanding,Qt::SmoothTransformation);
        p.drawPixmap(-image.width()/2,-image.height()/2,image); p.setClipping(false);
    }
    p.end();
    if (overlay_ && !painted_) { painted_ = true; Q_EMIT firstPaint(session_,monotonicNanos()); }
}
}
