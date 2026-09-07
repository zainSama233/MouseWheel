#pragma once
#include <QImage>
#include <QColor>
#include <QPointF>
#include <QPainter>
#include <QUndoStack>
#include <QVector>
namespace wheel {
enum class AnnotationTool { Pen, Rectangle, Arrow, Text, Mosaic };
struct Annotation {
    AnnotationTool tool=AnnotationTool::Pen;
    QVector<QPointF> points;
    QColor color=Qt::red;
    int width=4;
    QString text;
};
class AnnotationDocument final : public QObject {
    Q_OBJECT
public:
    explicit AnnotationDocument(QImage image);
    void add(Annotation annotation);
    const QImage& image() const { return rendered_; }
    QUndoStack& history() { return history_; }
    bool savePng(const QString& path, QString& error) const;
    static void paint(QPainter& painter, const Annotation& annotation, const QImage& source);
Q_SIGNALS:
    void changed();
private:
    class Command;
    void rebuild();
    QImage original_, rendered_;
    QVector<Annotation> annotations_;
    QUndoStack history_;
};
}
