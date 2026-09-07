#include "tools/pinned_image.h"
#include "ui/theme.h"
#include <QToolBar>
#include <QAction>
#include <QApplication>
#include <QClipboard>
#include <QPainter>
#include <QMouseEvent>
#include <QScreen>
#include <QFileDialog>
#include <QMessageBox>
#include <QSaveFile>
#include <QImageWriter>
#include <algorithm>
#include <cmath>
namespace wheel {
PinnedImage::PinnedImage(QImage image,Theme theme) : image_(std::move(image)),toolbar_(new QToolBar(this)) {
    image_.setDevicePixelRatio(1);
    setObjectName("pinned-image"); setWindowTitle(QStringLiteral("截图贴图"));
    setWindowFlags(Qt::Tool|Qt::FramelessWindowHint|Qt::WindowStaysOnTopHint);
    setAttribute(Qt::WA_DeleteOnClose); setStyleSheet(settingsStyle(theme));
    auto* copy=toolbar_->addAction(QStringLiteral("复制")); copy->setObjectName("copy-image");
    connect(copy,&QAction::triggered,this,[this]{QApplication::clipboard()->setImage(image_);});
    toolbar_->addAction(QStringLiteral("保存 PNG"),this,[this]{
        const auto path=QFileDialog::getSaveFileName(this,QStringLiteral("保存截图"),QStringLiteral("截图.png"),QStringLiteral("PNG 图片 (*.png)"));
        if(path.isEmpty()) return;
        QString error; if(!savePng(path,error)) QMessageBox::warning(this,QStringLiteral("保存失败"),error);
    });
    toolbar_->addAction(QStringLiteral("关闭"),this,&QWidget::close);
    setToolTip(QStringLiteral("拖动贴图 · 滚轮缩放"));
    const auto available=screen()->availableGeometry().size();
    scale_=std::min({1.0/screen()->devicePixelRatio(),double(available.width()-48)/image_.width(),double(available.height()-96)/image_.height()});
    resize(qMax(toolbar_->sizeHint().width(),qRound(image_.width()*scale_)),qRound(image_.height()*scale_)+toolbar_->sizeHint().height());
}
void PinnedImage::resizeEvent(QResizeEvent*) { toolbar_->setGeometry(0,0,width(),toolbar_->sizeHint().height()); }
void PinnedImage::paintEvent(QPaintEvent*) {
    QPainter p(this); p.fillRect(rect(),palette().window()); p.setRenderHint(QPainter::SmoothPixmapTransform);
    const QSizeF size(image_.width()*scale_,image_.height()*scale_);
    p.drawImage(QRectF(QPointF((width()-size.width())/2,toolbar_->height()),size),image_);
}
void PinnedImage::mousePressEvent(QMouseEvent* e) {
    if(e->button()==Qt::LeftButton) { dragging_=true; dragOffset_=e->globalPosition().toPoint()-pos(); }
}
void PinnedImage::mouseMoveEvent(QMouseEvent* e) { if(dragging_) move(e->globalPosition().toPoint()-dragOffset_); }
void PinnedImage::mouseReleaseEvent(QMouseEvent* e) { if(e->button()==Qt::LeftButton) dragging_=false; }
void PinnedImage::wheelEvent(QWheelEvent* e) {
    scale_=std::clamp(scale_*std::pow(1.15,e->angleDelta().y()/120.0),0.1,4.0);
    resize(qMax(toolbar_->sizeHint().width(),qRound(image_.width()*scale_)),qRound(image_.height()*scale_)+toolbar_->sizeHint().height());
    update(); e->accept();
}
bool PinnedImage::savePng(const QString& path,QString& error) const {
    error.clear(); QSaveFile file(path); file.setDirectWriteFallback(false);
    if(!file.open(QIODevice::WriteOnly)) { error=file.errorString(); return false; }
    QImageWriter writer(&file,"png");
    if(!writer.write(image_)) { error=writer.errorString(); return false; }
    if(!file.commit()) { error=file.errorString(); return false; }
    return true;
}
}
