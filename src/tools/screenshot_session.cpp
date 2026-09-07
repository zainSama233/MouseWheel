#include "tools/screenshot_session.h"
#include "tools/image_editor.h"
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
#include <Windows.h>
#include <dwmapi.h>
namespace wheel {
class RegionPicker final : public QDialog {
public:
    explicit RegionPicker(QScreen* screen,QImage image,Theme theme) : image_(std::move(image)),accent_(themeColors(theme).accent) {
        setObjectName("region-picker"); setWindowTitle(QStringLiteral("框选截图"));
        setWindowFlags(Qt::Tool|Qt::FramelessWindowHint|Qt::WindowStaysOnTopHint);
        setScreen(screen); setGeometry(screen->geometry()); setCursor(Qt::CrossCursor);
        auto* cancel=new QPushButton(QStringLiteral("取消截图"),this); cancel->move(16,16);
        cancel->setStyleSheet(settingsStyle(theme));
        connect(cancel,&QPushButton::clicked,this,&QDialog::reject);
    }
    QImage selectedImage() const {
        const auto region=selection_.normalized();
        const double sx=double(image_.width())/width(), sy=double(image_.height())/height();
        const auto pixels=QRectF(region.x()*sx,region.y()*sy,region.width()*sx,region.height()*sy).toAlignedRect().intersected(image_.rect());
        auto result=image_.copy(pixels); result.setDevicePixelRatio(1); return result;
    }
protected:
    void paintEvent(QPaintEvent*) override {
        QPainter p(this); p.drawImage(rect(),image_);
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
        const auto region=selection_.normalized();
        if(region.width()>=2 && region.height()>=2) accept();
    }
private:
    QImage image_;
    QColor accent_;
    QPointF origin_;
    QRectF selection_;
    bool dragging_=false;
};
ScreenshotSession::ScreenshotSession(QObject* parent) : QObject(parent) {}
ScreenshotSession::~ScreenshotSession() {
    clearPickers();
    if(editor_) { disconnect(editor_,nullptr,this,nullptr); delete editor_.data(); }
}
bool ScreenshotSession::active() const { return !pickers_.isEmpty() || !editor_.isNull(); }
void ScreenshotSession::clearPickers() {
    const auto pickers=std::exchange(pickers_,{});
    for(auto* picker:pickers) { disconnect(picker,nullptr,this,nullptr); delete picker; }
}
bool ScreenshotSession::start(Theme theme,QString& error) {
    error.clear(); if(active()) return false;
    if(FAILED(DwmFlush())) { error=QStringLiteral("无法同步桌面画面，请重试。"); return false; }
    for(auto* screen:QGuiApplication::screens()) {
        const auto image=screen->grabWindow(0).toImage();
        if(image.isNull()) { clearPickers(); error=QStringLiteral("无法读取屏幕画面。"); return false; }
        auto* picker=new RegionPicker(screen,image,theme); pickers_.append(picker);
        connect(screen,&QScreen::geometryChanged,picker,&QDialog::reject);
        connect(screen,&QObject::destroyed,picker,&QDialog::reject);
        connect(picker,&QDialog::finished,this,[this,picker,theme](int result){
            const QPointer<QScreen> screen=picker->screen();
            QImage selected=result==QDialog::Accepted ? picker->selectedImage() : QImage();
            for(auto* window:pickers_) window->hide();
            QTimer::singleShot(0,this,[this,selected,theme,screen]{
                if(pickers_.isEmpty()) return;
                clearPickers();
                if(!selected.isNull()) {
                    editor_=new ImageEditor(selected,theme);
                    if(screen) { editor_->setScreen(screen); editor_->move(screen->availableGeometry().topLeft()+QPoint(24,24)); }
                    connect(editor_,&QObject::destroyed,this,[this]{editor_=nullptr; Q_EMIT activeChanged();});
                    editor_->show(); editor_->raise(); editor_->activateWindow();
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
