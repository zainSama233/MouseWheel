#pragma once
#include <QWidget>
#include "tools/annotation_document.h"
#include "core/model.h"
namespace wheel {
class AnnotationCanvas final : public QWidget {
    Q_OBJECT
public:
    explicit AnnotationCanvas(AnnotationDocument& document, QWidget* parent=nullptr);
    void setTool(AnnotationTool tool) { tool_=tool; }
    void setColor(QColor color) { color_=color; }
    void setWidth(int width) { width_=width; }
    QRectF imageRect() const;
protected:
    void paintEvent(QPaintEvent*) override;
    void mousePressEvent(QMouseEvent*) override;
    void mouseMoveEvent(QMouseEvent*) override;
    void mouseReleaseEvent(QMouseEvent*) override;
private:
    QPointF imagePoint(QPointF point) const;
    AnnotationDocument& document_;
    AnnotationTool tool_=AnnotationTool::Pen;
    QColor color_=QColor("#ef4444");
    int width_=4;
    std::optional<Annotation> pending_;
};
class ImageEditor final : public QWidget {
    Q_OBJECT
public:
    explicit ImageEditor(QImage image, Theme theme);
    AnnotationDocument& document() { return document_; }
protected:
    void keyPressEvent(QKeyEvent*) override;
private:
    AnnotationDocument document_;
};
}
