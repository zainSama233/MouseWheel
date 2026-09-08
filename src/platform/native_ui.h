#pragma once
#include "core/model.h"
#include <QImage>
class QWidget;
namespace wheel::platform {
void initializeDisplay();
bool overlayEvent(void* message,qintptr* result);
void setOverlayInput(QWidget* widget,bool transparent);
void placeOverlay(QWidget* widget,const Geometry& geometry,bool visible);
QImage captureBackdrop(QWidget* widget,const Geometry& geometry);
bool prepareCapture(QString& error);
QString defaultConfigPath();
}
