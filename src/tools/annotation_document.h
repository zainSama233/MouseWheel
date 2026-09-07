#pragma once
#include <QColor>
#include <QPointF>
#include <QPainter>
#include <QUndoStack>
#include <QVector>
namespace wheel {
enum class AnnotationTool { Pen, Rectangle, Arrow, Text, Highlighter, Eraser };
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
    AnnotationDocument();
    void add(Annotation annotation);
    void clear();
    void paint(QPainter& painter) const;
    QUndoStack& history() { return history_; }
    static void paint(QPainter& painter, const Annotation& annotation);
Q_SIGNALS:
    void changed();
private:
    class Command;
    QVector<Annotation> annotations_;
    QUndoStack history_;
};
}
