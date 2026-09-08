#include <QCoreApplication>
#include "ui/wheel_window.h"
#include "ui/theme.h"
#include "core/screen_helper.h"
#include <QStyleHints>
#include <QGraphicsScene>
#include <QGraphicsPixmapItem>
#include <QGraphicsBlurEffect>
#include <QWheelEvent>
#include <cmath>
#include "ui/action_icons.h"
#include "core/image_asset.h"
#include "core/clock.h"
#include <QPainter>
#include <QPainterPath>
#include <QGuiApplication>
#include <QScreen>
#include <QMouseEvent>
#include <QApplication>
#include "platform/native_ui.h"
namespace wheel {
namespace {
QPixmap blurredBackdrop(QPixmap source) {
    source.setDevicePixelRatio(1);QGraphicsScene scene;auto* item=scene.addPixmap(source);auto* blur=new QGraphicsBlurEffect;
    blur->setBlurRadius(24);blur->setBlurHints(QGraphicsBlurEffect::QualityHint);item->setGraphicsEffect(blur);
    QPixmap result(source.size());result.fill(Qt::transparent);QPainter painter(&result);scene.render(&painter,QRectF(result.rect()),QRectF(source.rect()));return result;
}
}
WheelWindow::WheelWindow(bool overlay, QWidget* parent) : QWidget(parent), opening_(this), overlay_(overlay) {
    dragHover_.setSingleShot(true);dragHover_.setInterval(500);
    connect(&dragHover_,&QTimer::timeout,this,[this]{Q_EMIT levelRequested(hoverGroup_);});
    connect(QGuiApplication::styleHints(),&QStyleHints::colorSchemeChanged,this,[this]{cached_=false;applyLevel();update();});
    opening_.setDuration(120); opening_.setStartValue(0.15); opening_.setEndValue(1.0);
    opening_.setEasingCurve(QEasingCurve::OutCubic);
    connect(&opening_,&QVariantAnimation::valueChanged,this,[this](const QVariant& value){opacity_=value.toDouble(); update();});
    if (overlay_) {
        setWindowFlags(Qt::Tool | Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint |
                       Qt::WindowDoesNotAcceptFocus | Qt::WindowTransparentForInput);
        setAttribute(Qt::WA_TranslucentBackground);
        setAttribute(Qt::WA_ShowWithoutActivating);
        setFocusPolicy(Qt::NoFocus);
        platform::setOverlayInput(this,true);
    }
    resize(int(WheelRadius*2),int(WheelRadius*2)); applyConfig(config_);
}
void WheelWindow::present(quint64 session, Config config, Geometry geometry, const QString& name) {
    if (session <= session_) return;
    extent_=geometry.extent;backdrop_=QPixmap{};
    painted_ = false; session_ = session; group_=-1;rootConfig_=config;applyLevel(); selection_ = -1;
    opening_.stop(); opacity_=0.15;
    for (auto* screen : QGuiApplication::screens()) {
        if (screen->name() != name) continue;
        setScreen(screen);
        break;
    }
    platform::placeOverlay(this,geometry,false);
    if(config_.frosted){hide();backdrop_=blurredBackdrop(QPixmap::fromImage(platform::captureBackdrop(this,geometry)));}
    platform::placeOverlay(this,geometry,true);
    opening_.start(); update();
}
void WheelWindow::changeLevel(quint64 session,int group) {
    if(session!=session_ || group==group_) return;
    group_=group;applyLevel();selection_=-1;opening_.stop();opacity_=0.15;opening_.start();update();
}
void WheelWindow::select(quint64 session, int index) {
    if (session != session_ || selection_ == index) return;
    selection_ = index; update();
}
void WheelWindow::dismiss(quint64 session) {
    if (session >= session_) { session_ = session; opening_.stop(); hide(); }
    Q_EMIT hidden(session);
}
void WheelWindow::applyLevel() {
    auto config=rootConfig_;
    if(group_>=0) {config.centerEnabled=false;config.slots=std::get<GroupAction>(rootConfig_.slots[group_].action).slots;config.centerImage.clear();}
    applyConfig(config);
}
void WheelWindow::applyConfig(const Config& config) {
    if(cached_ && cachedGroup_==group_ && config_==config) return;
    cachedGroup_=group_;
    config_=config; const auto colors=themeColors(config.theme);
    icons_.resize(config.slots.size());selectedIcons_.resize(config.slots.size());
    for(int i=0;i<config.slots.size();++i) {
        const auto style=cascadeStyle(config.style,config.slots[i].style);
        icons_[i]=actionIcon(config.slots[i],style.text.value_or(config.slots[i].enabled()?colors.text:colors.muted),assetDirectory_);
        selectedIcons_[i]=actionIcon(config.slots[i],style.text.value_or(colors.selectedText),assetDirectory_);
    }
    const auto centerStyle=cascadeStyle(config.style,config.center.style);
    centerIcon_=actionIcon(config.center,centerStyle.text.value_or(colors.text),assetDirectory_);selectedCenterIcon_=actionIcon(config.center,centerStyle.text.value_or(colors.selectedText),assetDirectory_);
    cancelIcon_=symbolIcon(group_>=0?"undo-2":"x",colors.muted);
    centerImage_=QPixmap::fromImage(decodeImageAsset(config.centerImage)); cached_=true;
}
void WheelWindow::preview(const Config& config,int group) {
    rootConfig_=config;extent_=ScreenHelper::extent(config);group_=group;applyLevel();
    if(!overlay_) {
        if(!config.frosted)backdrop_={};
        else if(backdrop_.isNull()){QPixmap sample(384,384);QPainter painter(&sample);QLinearGradient gradient(0,0,384,384);gradient.setColorAt(0,QColor("#a1c4fd"));gradient.setColorAt(1,QColor("#fbc2eb"));painter.fillRect(sample.rect(),gradient);painter.setPen(QPen(QColor("#346fa2"),16));for(int y=30;y<384;y+=55)painter.drawLine(0,y,384,y+80);painter.end();backdrop_=blurredBackdrop(sample);}
    }
    update();
}
void WheelWindow::resetView() {zoom_=1;pan_={};update();}
QTransform WheelWindow::viewTransform() const {
    QTransform transform;transform.translate(width()/2.0+pan_.x(),height()/2.0+pan_.y());
    const double scale=qMin(width(),height())/(extent_*2)*zoom_;transform.scale(scale,scale);return transform;
}
int WheelWindow::previewSlotAt(QPointF position) const {
    const auto local=viewTransform().inverted().map(position);
    if(QLineF(local,{}).length()<(group_<0?config_.deadZone:CenterRadius))return -2;
    return Geometry{{0,0}}.hit(local,config_.shape,config_.slots.size());
}
void WheelWindow::wheelEvent(QWheelEvent* event) {
    if(overlay_)return;
    const auto local=viewTransform().inverted().map(event->position());zoom_=qBound(.4,zoom_*std::pow(1.15,event->angleDelta().y()/120.0),3.);
    pan_+=event->position()-viewTransform().map(local);update();event->accept();
}
void WheelWindow::mousePressEvent(QMouseEvent* event) {
    if(overlay_)return;
    if(event->button()==Qt::MiddleButton){panning_=true;panStart_=event->position();setCursor(Qt::ClosedHandCursor);return;}
    if(event->button()!=Qt::LeftButton)return;
    dragStart_=event->position();dragSource_=previewSlotAt(dragStart_);dragGroup_=group_;
    if(group_>=0 && dragSource_==-2)dragSource_=-1;
    if(dragSource_!=-1 && (group_<0 || dragSource_!=-2))Q_EMIT slotClicked(dragSource_);
}
void WheelWindow::mouseMoveEvent(QMouseEvent* event) {
    if(overlay_)return;
    if(panning_){pan_+=event->position()-panStart_;panStart_=event->position();update();return;}
    if(dragSource_==-1 || !(event->buttons()&Qt::LeftButton))return;
    if((event->position()-dragStart_).manhattanLength()<QApplication::startDragDistance())return;
    setCursor(Qt::ClosedHandCursor);const int target=previewSlotAt(event->position());select(0,target);
    int next=-2;
    if(group_>=0 && target==-2)next=-1;
    else if(group_<0 && target>=0 && config_.slots[target].kind()==ActionKind::Group)next=target;
    if(next!=hoverGroup_){dragHover_.stop();hoverGroup_=next;if(next!=-2)dragHover_.start();}
}
void WheelWindow::mouseReleaseEvent(QMouseEvent* event) {
    if(overlay_)return;
    if(event->button()==Qt::MiddleButton){panning_=false;unsetCursor();return;}
    if(event->button()!=Qt::LeftButton)return;
    dragHover_.stop();hoverGroup_=-2;const int source=std::exchange(dragSource_,-1);unsetCursor();
    if(source==-1){if(group_>=0 && previewSlotAt(event->position())==-2 && (event->position()-dragStart_).manhattanLength()<QApplication::startDragDistance())Q_EMIT levelRequested(-1);return;}
    if((event->position()-dragStart_).manhattanLength()<QApplication::startDragDistance()) {
        if(group_>=0 && source==-2)Q_EMIT levelRequested(-1);return;
    }
    const int target=previewSlotAt(event->position());if(target==-1 || (group_>=0 && target==-2))return;
    if(dragGroup_==group_) {if(target!=source)Q_EMIT slotsSwapped(source,target);}
    else Q_EMIT positionsSwapped(dragGroup_,source,group_,target);
}
bool WheelWindow::nativeEvent(const QByteArray& type, void* message, qintptr* result) {
    if(overlay_ && platform::overlayEvent(message,result))return true;
    return QWidget::nativeEvent(type,message,result);
}
void WheelWindow::paintEvent(QPaintEvent*) {
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);
    const auto colors = themeColors(config_.theme);
    p.setTransform(viewTransform());
    p.setOpacity(opacity_);
    const double scale=0.94+0.06*opacity_;p.scale(scale,scale);
    const auto drawSlot=[&](const Slot& slot,const QPainterPath& path,QPointF center,bool selected,const QIcon& icon) {
        const auto style=cascadeStyle(config_.style,slot.style);
        const double glow=style.glowRadius.value_or(0);
        if(selected && glow>0) {
            for(int radius=int(std::ceil(glow));radius>0;--radius){auto color=style.glow.value_or(colors.accent);color.setAlphaF(color.alphaF()*.12*(1.-radius/(glow+1)));p.strokePath(path,QPen(color,radius*2,Qt::SolidLine,Qt::RoundCap,Qt::RoundJoin));}
        }
        QColor fill=style.fill.value_or(selected?colors.selected:colors.surface);
        if(config_.frosted && !backdrop_.isNull()) {p.save();p.setClipPath(path);p.drawPixmap(QRectF(-extent_,-extent_,extent_*2,extent_*2),backdrop_,backdrop_.rect());p.restore();fill.setAlphaF(.72*fill.alphaF());}
        p.fillPath(path,fill);
        if(style.borderWidth.value_or(0)>0)p.strokePath(path,QPen(style.border.value_or(colors.accent),*style.borderWidth));
        const auto content=ScreenHelper::content(slot,style,center);icon.paint(&p,content.icon.toAlignedRect());
        if(!content.label.isEmpty()){p.setFont(content.font);p.setPen(style.text.value_or(selected?colors.selectedText:colors.muted));p.drawText(content.text,Qt::AlignCenter,content.label);}
        p.setPen(Qt::NoPen);
    };
    for(int i=0;i<config_.slots.size();++i) {
        const bool selected=i==selection_ && (!overlay_ || config_.slots[i].enabled());
        drawSlot(config_.slots[i],slotPath(config_.shape,i,config_.slots.size()),slotCenter(i,config_.slots.size(),config_.shape),selected,selected?selectedIcons_[i]:icons_[i]);
    }
    const double radius=group_<0?config_.deadZone:CenterRadius;const QRectF inner(-radius,-radius,radius*2,radius*2);QPainterPath centerPath;centerPath.addEllipse(inner);
    if(group_<0 && config_.centerEnabled)drawSlot(config_.center,centerPath,{},selection_==-2,selection_==-2?selectedCenterIcon_:centerIcon_);
    else {
        p.fillPath(centerPath,colors.surface);
        if(group_<0 && config_.center.icon.source!=IconSource::Automatic && config_.center.icon.source!=IconSource::Program)centerIcon_.paint(&p,QRect(-24,-24,48,48));
        else if(centerImage_.isNull())cancelIcon_.paint(&p,QRect(-12,-12,24,24));
        else {p.save();QPainterPath clip;clip.addEllipse(inner.adjusted(4,4,-4,-4));p.setClipPath(clip);const int diameter=qRound((radius-4)*2);const auto image=centerImage_.scaled(diameter,diameter,Qt::KeepAspectRatioByExpanding,Qt::SmoothTransformation);p.drawPixmap(-image.width()/2,-image.height()/2,image);p.restore();}
    }
    if(group_>=0) {
        auto font=p.font();font.setPixelSize(10);p.setFont(font);p.setPen(colors.muted);
        p.drawText(QRectF(-38,14,76,16),Qt::AlignCenter,QCoreApplication::translate("MouseWheel","返回"));
        p.drawText(QRectF(-34,-32,68,16),Qt::AlignCenter,p.fontMetrics().elidedText(rootConfig_.slots[group_].name,Qt::ElideRight,64));
    }
    p.end();
    if (overlay_ && !painted_) { painted_ = true; Q_EMIT firstPaint(session_,monotonicNanos()); }
}
}
