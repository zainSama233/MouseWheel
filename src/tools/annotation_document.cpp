#include "tools/annotation_document.h"
#include <QSaveFile>
#include <QImageWriter>
#include <QPainterPath>
#include <cmath>
namespace wheel {
class AnnotationDocument::Command final : public QUndoCommand {
public:
    Command(AnnotationDocument& document, Annotation annotation) : document_(document), annotation_(std::move(annotation)) {}
    void redo() override { document_.annotations_.append(annotation_); document_.rebuild(); }
    void undo() override { document_.annotations_.removeLast(); document_.rebuild(); }
private:
    AnnotationDocument& document_;
    Annotation annotation_;
};
AnnotationDocument::AnnotationDocument(QImage image) : original_(std::move(image)) {
    original_.setDevicePixelRatio(1); rendered_=original_;
    history_.setUndoLimit(100);
}
void AnnotationDocument::add(Annotation annotation) {
    if (annotation.points.isEmpty() || (annotation.tool==AnnotationTool::Text && annotation.text.trimmed().isEmpty())) return;
    history_.push(new Command(*this,std::move(annotation)));
}
void AnnotationDocument::rebuild() {
    rendered_=original_.copy();
    for (const auto& annotation : annotations_) {
        const QImage source=annotation.tool==AnnotationTool::Mosaic ? rendered_.copy() : QImage();
        QPainter painter(&rendered_); paint(painter,annotation,source);
    }
    Q_EMIT changed();
}
void AnnotationDocument::paint(QPainter& p, const Annotation& a, const QImage& source) {
    if(a.points.isEmpty()) return;
    p.save(); p.setRenderHint(QPainter::Antialiasing);
    p.setPen(QPen(a.color,a.width,Qt::SolidLine,Qt::RoundCap,Qt::RoundJoin)); p.setBrush(Qt::NoBrush);
    const auto first=a.points.first(), last=a.points.last();
    const auto rect=QRectF(first,last).normalized();
    switch(a.tool) {
    case AnnotationTool::Pen:
        if(a.points.size()==1) p.drawPoint(first); else p.drawPolyline(a.points.constData(),a.points.size());
        break;
    case AnnotationTool::Rectangle: p.drawRect(rect); break;
    case AnnotationTool::Arrow: {
        QLineF line(first,last); p.drawLine(line);
        if(line.length()>0) {
            const auto unit=(last-first)/line.length(); const QPointF normal(-unit.y(),unit.x());
            const double size=qMax(12,a.width*3);
            p.drawLine(last,last-unit*size+normal*size*0.5);
            p.drawLine(last,last-unit*size-normal*size*0.5);
        }
        break;
    }
    case AnnotationTool::Text: {
        auto font=p.font(); font.setPixelSize(qMax(16,a.width*5)); p.setFont(font);
        p.drawText(first,a.text); break;
    }
    case AnnotationTool::Mosaic: {
        const auto region=rect.toAlignedRect().intersected(source.rect());
        if(!region.isEmpty()) {
            auto patch=source.copy(region);
            patch=patch.scaled(qMax(1,region.width()/12),qMax(1,region.height()/12),Qt::IgnoreAspectRatio,Qt::SmoothTransformation);
            p.setRenderHint(QPainter::SmoothPixmapTransform,false);
            p.drawImage(region,patch.scaled(region.size(),Qt::IgnoreAspectRatio,Qt::FastTransformation));
        }
        break;
    }
    }
    p.restore();
}
bool AnnotationDocument::savePng(const QString& path, QString& error) const {
    error.clear(); QSaveFile file(path); file.setDirectWriteFallback(false);
    if(!file.open(QIODevice::WriteOnly)) { error=file.errorString(); return false; }
    QImageWriter writer(&file,"png");
    if(!writer.write(rendered_)) { error=writer.errorString(); return false; }
    if(!file.commit()) { error=file.errorString(); return false; }
    return true;
}
}
