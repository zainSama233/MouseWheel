#pragma once
#include <QWidget>
#include <QImage>
#include "core/model.h"
class QToolBar;
namespace wheel {
class PinnedImage final : public QWidget {
    Q_OBJECT
public:
    PinnedImage(QImage image,Theme theme);
    const QImage& image() const { return image_; }
    bool savePng(const QString& path,QString& error) const;
protected:
    void paintEvent(QPaintEvent*) override;
    void resizeEvent(QResizeEvent*) override;
    void mousePressEvent(QMouseEvent*) override;
    void mouseMoveEvent(QMouseEvent*) override;
    void mouseReleaseEvent(QMouseEvent*) override;
    void wheelEvent(QWheelEvent*) override;
private:
    QImage image_;
    QToolBar* toolbar_;
    QPoint dragOffset_;
    bool dragging_=false;
    double scale_=1;
};
}
