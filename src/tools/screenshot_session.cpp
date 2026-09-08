#include "tools/screenshot_session.h"
#include "tools/pinned_image.h"
#include <QScreen>
#include <utility>
namespace wheel {
ScreenshotSession::ScreenshotSession(QObject* parent):QObject(parent) {
    connect(&capture_,&RegionCapture::activeChanged,this,&ScreenshotSession::activeChanged);
    connect(&capture_,&RegionCapture::selected,this,[this](QImage image,QScreen* screen){
        auto* pin=new PinnedImage(image,theme_); pins_.append(pin);
        if(screen) {pin->setScreen(screen);pin->move(screen->availableGeometry().topLeft()+QPoint(24,24));}
        connect(pin,&QObject::destroyed,this,[this,pin]{pins_.removeAll(pin);});
        pin->show();pin->raise();pin->activateWindow();
    });
}
ScreenshotSession::~ScreenshotSession() { for(auto* pin:std::exchange(pins_,{})) {disconnect(pin,nullptr,this,nullptr);delete pin;} }
bool ScreenshotSession::active() const {return capture_.active();}
void ScreenshotSession::cancel() {capture_.cancel();}
bool ScreenshotSession::start(Theme theme,QString& error) {theme_=theme;return capture_.start(theme,error);}
}
