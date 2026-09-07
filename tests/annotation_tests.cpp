#include <QtTest>
#include "tools/annotation_document.h"
using namespace wheel;
class AnnotationTests final : public QObject {
    Q_OBJECT
    QImage render(const AnnotationDocument& document) {
        QImage image(200,100,QImage::Format_ARGB32_Premultiplied); image.fill(Qt::transparent);
        QPainter painter(&image); document.paint(painter); return image;
    }
private Q_SLOTS:
    void transparentHistory() {
        AnnotationDocument document; const auto empty=render(document);
        Annotation a; a.points={{10,20},{100,20}}; a.color=Qt::blue;
        document.add(a); const auto painted=render(document);
        QCOMPARE(painted.pixelColor(50,20),QColor(Qt::blue)); QCOMPARE(painted.pixelColor(150,80).alpha(),0);
        document.history().undo(); QCOMPARE(render(document),empty);
        document.history().redo(); QCOMPARE(render(document),painted);
        document.clear(); QCOMPARE(render(document),empty);
        document.history().undo(); QCOMPARE(render(document),painted);
        document.history().undo(); a.tool=AnnotationTool::Arrow; document.add(a);
        QVERIFY(!document.history().canRedo()); QVERIFY(render(document)!=empty);
    }
    void highlightAndEraser() {
        AnnotationDocument document;
        Annotation a; a.tool=AnnotationTool::Highlighter; a.points={{10,30},{150,30}}; a.width=4;
        document.add(a); const auto highlighted=render(document);
        QVERIFY(highlighted.pixelColor(50,30).alpha()>0); QVERIFY(highlighted.pixelColor(50,30).alpha()<150);
        a.tool=AnnotationTool::Eraser; a.points={{50,10},{50,60}}; document.add(a);
        QCOMPARE(render(document).pixelColor(50,30).alpha(),0);
        document.history().undo(); QCOMPARE(render(document),highlighted);
    }
    void shapesAndText() {
        for(auto tool:{AnnotationTool::Rectangle,AnnotationTool::Arrow,AnnotationTool::Text}) {
            AnnotationDocument document; const auto empty=render(document);
            Annotation a; a.tool=tool; a.points={{20,40},{150,80}}; a.text=QStringLiteral("标注");
            document.add(a); QVERIFY(render(document)!=empty);
            document.history().undo(); QCOMPARE(render(document),empty);
        }
    }
};
QTEST_MAIN(AnnotationTests)
#include "annotation_tests.moc"
