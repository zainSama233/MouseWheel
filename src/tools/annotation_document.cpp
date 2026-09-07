#include "tools/annotation_document.h"
namespace wheel {
class AnnotationDocument::Command final : public QUndoCommand {
public:
    Command(AnnotationDocument& document,QVector<Annotation> next)
        : document_(document),before_(document.annotations_),after_(std::move(next)) {}
    void redo() override { document_.annotations_=after_; Q_EMIT document_.changed(); }
    void undo() override { document_.annotations_=before_; Q_EMIT document_.changed(); }
private:
    AnnotationDocument& document_;
    QVector<Annotation> before_,after_;
};
AnnotationDocument::AnnotationDocument() { history_.setUndoLimit(100); }
void AnnotationDocument::add(Annotation annotation) {
    if(annotation.points.isEmpty() || (annotation.tool==AnnotationTool::Text && annotation.text.trimmed().isEmpty())) return;
    auto next=annotations_; next.append(std::move(annotation));
    history_.push(new Command(*this,std::move(next)));
}
void AnnotationDocument::clear() {
    if(!annotations_.isEmpty()) history_.push(new Command(*this,{}));
}
void AnnotationDocument::paint(QPainter& painter) const {
    for(const auto& annotation:annotations_) paint(painter,annotation);
}
void AnnotationDocument::paint(QPainter& p, const Annotation& a) {
    if(a.points.isEmpty()) return;
    p.save(); p.setRenderHint(QPainter::Antialiasing);
    QColor color=a.color;
    int width=a.width;
    if(a.tool==AnnotationTool::Highlighter) { color.setAlpha(90); width*=4; }
    if(a.tool==AnnotationTool::Eraser) { p.setCompositionMode(QPainter::CompositionMode_Clear); width*=4; }
    p.setPen(QPen(color,width,Qt::SolidLine,Qt::RoundCap,Qt::RoundJoin)); p.setBrush(Qt::NoBrush);
    const auto first=a.points.first(), last=a.points.last();
    const auto rect=QRectF(first,last).normalized();
    switch(a.tool) {
    case AnnotationTool::Highlighter:
    case AnnotationTool::Eraser:
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
    }
    p.restore();
}
}
