#include "platform/native_ui.h"
#include <QWidget>
#include <QScreen>
#include <QStandardPaths>
#include <QDir>
#import <AppKit/AppKit.h>
namespace wheel::platform {
void initializeDisplay(){}
bool overlayEvent(void*,qintptr*){return false;}
void setOverlayInput(QWidget* widget,bool transparent) {
    NSWindow* window=[(__bridge NSView*)reinterpret_cast<void*>(widget->winId()) window];
    window.ignoresMouseEvents=transparent;window.hidesOnDeactivate=NO;
    window.level=NSStatusWindowLevel;window.collectionBehavior=NSWindowCollectionBehaviorCanJoinAllSpaces|NSWindowCollectionBehaviorFullScreenAuxiliary;
}
void placeOverlay(QWidget* widget,const Geometry& geometry,bool visible) {
    widget->setGeometry(qRound(geometry.center.x()-geometry.radius),qRound(geometry.center.y()-geometry.radius),qRound(geometry.radius*2),qRound(geometry.radius*2));
    if(visible){widget->show();setOverlayInput(widget,true);NSWindow* window=[(__bridge NSView*)reinterpret_cast<void*>(widget->winId()) window];[window orderFrontRegardless];}
}
QImage captureBackdrop(QWidget* widget,const Geometry& geometry) {
    if(!CGPreflightScreenCaptureAccess())return {};
    const auto screen=widget->screen();const auto scale=screen->devicePixelRatio();const auto origin=geometry.center-QPointF(geometry.radius,geometry.radius)-screen->geometry().topLeft();
    auto image=screen->grabWindow(0).toImage().copy(QRect(qRound(origin.x()*scale),qRound(origin.y()*scale),qRound(geometry.radius*2*scale),qRound(geometry.radius*2*scale)));
    image.setDevicePixelRatio(1);return image;
}
bool prepareCapture(QString& error) {
    if(CGPreflightScreenCaptureAccess() || CGRequestScreenCaptureAccess())return true;
    error=QStringLiteral("请在系统设置 → 隐私与安全性 → 屏幕录制中允许 MouseWheel，然后重新启动。");return false;
}
QString defaultConfigPath(){const auto path=QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);QDir().mkpath(path);return path+"/config.json";}
}
