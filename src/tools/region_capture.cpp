#include <QCoreApplication>
#include "tools/region_capture.h"
#include <QPointer>

#include "ui/theme.h"
#include <utility>
#include <algorithm>
#include <QGuiApplication>
#include <QScreen>
#include <QDialog>
#include <QPushButton>
#include <QPainter>
#include <QMouseEvent>
#include <QTimer>
#include "platform/native_ui.h"

namespace wheel {
class RegionPicker final : public QDialog {
public:
    explicit RegionPicker(QScreen* screen,QImage image,Theme theme,RegionCapture::Mode mode) : mode_(mode),image_(std::move(image)),accent_(themeColors(theme).accent) {
        setObjectName("region-picker"); setWindowTitle(QCoreApplication::translate("MouseWheel","框选屏幕"));
        setWindowFlags(Qt::Tool|Qt::FramelessWindowHint|Qt::WindowStaysOnTopHint);
        setScreen(screen); setGeometry(screen->geometry()); setCursor(Qt::CrossCursor);
        auto* cancel=new QPushButton(QCoreApplication::translate("MouseWheel","取消框选"),this); cancel->move(16,16);
        cancel->setStyleSheet(settingsStyle(theme));
        connect(cancel,&QPushButton::clicked,this,&QDialog::reject);
    }
    QImage selectedImage() const {
        if(mode_==RegionCapture::Mode::Pixel) {
            const QPoint pixel(qBound(0,int(origin_.x()*image_.width()/width()),image_.width()-1),qBound(0,int(origin_.y()*image_.height()/height()),image_.height()-1));
            auto result=image_.copy(QRect(pixel,QSize(1,1)));result.setDevicePixelRatio(1);return result;
        }
        const auto region=selection_.normalized();
        const double sx=double(image_.width())/width(), sy=double(image_.height())/height();
        const auto pixels=QRectF(region.x()*sx,region.y()*sy,region.width()*sx,region.height()*sy).toAlignedRect().intersected(image_.rect());
        auto result=image_.copy(pixels); result.setDevicePixelRatio(1); return result;
    }
protected:
    void paintEvent(QPaintEvent*) override {
        QPainter p(this); p.drawImage(rect(),image_);
        if(mode_==RegionCapture::Mode::Pixel)return;
        const auto selection=selection_.normalized().toAlignedRect().intersected(rect());
        const QRegion outside=QRegion(rect()).subtracted(QRegion(selection));
        p.setClipRegion(outside); p.fillRect(rect(),QColor(0,0,0,100)); p.setClipping(false);
        p.setPen(QPen(accent_,2)); p.drawRect(selection);
    }
    void mousePressEvent(QMouseEvent* event) override {
        if(event->button()==Qt::RightButton) { reject(); return; }
        if(event->button()!=Qt::LeftButton) return;
        origin_=event->position(); selection_=QRectF(origin_,origin_); dragging_=true; update();
    }
    void mouseMoveEvent(QMouseEvent* event) override {
        if(!dragging_) return;
        const auto position=QPointF(std::clamp(event->position().x(),0.0,double(width())),std::clamp(event->position().y(),0.0,double(height())));
        selection_=QRectF(origin_,position); update();
    }
    void mouseReleaseEvent(QMouseEvent* event) override {
        if(event->button()!=Qt::LeftButton || !dragging_) return;
        mouseMoveEvent(event); dragging_=false;
        if(mode_==RegionCapture::Mode::Pixel){accept();return;}
        const auto region=selection_.normalized();
        if(region.width()>=2 && region.height()>=2) accept();
    }
private:
    RegionCapture::Mode mode_;
    QImage image_;
    QColor accent_;
    QPointF origin_;
    QRectF selection_;
    bool dragging_=false;
};
RegionCapture::RegionCapture(QObject* parent) : QObject(parent) {}
RegionCapture::~RegionCapture() {
    clearPickers();

}
bool RegionCapture::active() const { return !pickers_.isEmpty(); }
void RegionCapture::cancel() {
    ++generation_; clearPickers(); Q_EMIT activeChanged();
}
void RegionCapture::clearPickers() {
    const auto pickers=std::exchange(pickers_,{});
    for(auto* picker:pickers) { disconnect(picker,nullptr,this,nullptr); delete picker; }
}
bool RegionCapture::start(Theme theme,QString& error,Mode mode) {
    error.clear(); if(active()) return false;
    if(!platform::prepareCapture(error))return false;
    const auto generation=++generation_;
    for(auto* screen:QGuiApplication::screens()) {
        const auto image=screen->grabWindow(0).toImage();
        if(image.isNull()) { clearPickers(); error=QCoreApplication::translate("MouseWheel","无法读取屏幕画面。"); return false; }
        auto* picker=new RegionPicker(screen,image,theme,mode); pickers_.append(picker);
        connect(screen,&QScreen::geometryChanged,picker,&QDialog::reject);
        connect(screen,&QObject::destroyed,picker,&QDialog::reject);
        connect(picker,&QDialog::finished,this,[this,picker,theme,generation](int result){
            const QPointer<QScreen> screen=picker->screen();
            QImage selected=result==QDialog::Accepted ? picker->selectedImage() : QImage();
            for(auto* window:pickers_) { disconnect(window,nullptr,this,nullptr); window->hide(); }
            QTimer::singleShot(0,this,[this,selected,theme,screen,generation]{
                if(generation!=generation_ || pickers_.isEmpty()) return;
                clearPickers();
                if(!selected.isNull()) {
                    Q_EMIT this->selected(selected,screen.data());
                }
                Q_EMIT activeChanged();
            });
        });
    }
    for(auto* picker:pickers_) picker->show();
    if(!pickers_.isEmpty()) pickers_.first()->activateWindow();
    Q_EMIT activeChanged(); return active();
}
}
