#include <QtTest>
#include <QTemporaryDir>
#include "tools/annotation_document.h"
using namespace wheel;
class AnnotationTests final : public QObject {
    Q_OBJECT
private Q_SLOTS:
    void penAndTextRender() {
        QImage base(200,100,QImage::Format_ARGB32_Premultiplied); base.fill(Qt::white);
        AnnotationDocument document(base);
        Annotation pen; pen.points={{10,20},{100,20}}; pen.color=Qt::blue; pen.width=4;
        document.add(pen); QCOMPARE(document.image().pixelColor(50,20),QColor(Qt::blue));
        document.history().undo(); QCOMPARE(document.image(),base);
        Annotation text; text.tool=AnnotationTool::Text; text.points={{20,50}}; text.text=QStringLiteral("截图");
        document.add(text); QVERIFY(document.image()!=base);
        document.history().undo(); QCOMPARE(document.image(),base);
    }
    void renderUndoAndBranch() {
        QImage base(120,80,QImage::Format_ARGB32_Premultiplied); base.fill(Qt::white);
        AnnotationDocument document(base);
        Annotation a; a.tool=AnnotationTool::Rectangle; a.points={{10,10},{70,50}}; a.color=Qt::red; a.width=4;
        document.add(a);
        QVERIFY(document.image()!=base);
        const auto painted=document.image();
        document.history().undo(); QCOMPARE(document.image(),base);
        document.history().redo(); QCOMPARE(document.image(),painted);
        document.history().undo(); a.tool=AnnotationTool::Arrow; document.add(a);
        QVERIFY(!document.history().canRedo()); QVERIFY(document.image()!=base);
        QCOMPARE(base.pixelColor(10,10),QColor(Qt::white));
    }
    void mosaicAndPng() {
        QImage base(100,80,QImage::Format_ARGB32_Premultiplied);
        for(int y=0;y<80;++y) for(int x=0;x<100;++x) base.setPixelColor(x,y,((x+y)%2)?Qt::white:Qt::black);
        AnnotationDocument document(base);
        Annotation a; a.tool=AnnotationTool::Mosaic; a.points={{10,10},{60,60}}; document.add(a);
        QCOMPARE(document.image().pixelColor(0,0),base.pixelColor(0,0));
        QVERIFY(document.image().copy(10,10,50,50)!=base.copy(10,10,50,50));
        QTemporaryDir dir; QString error;
        QVERIFY(document.savePng(dir.filePath("capture.png"),error));
        QCOMPARE(QImage(dir.filePath("capture.png")).convertToFormat(document.image().format()),document.image());
        QVERIFY(!document.savePng(dir.filePath("missing/capture.png"),error)); QVERIFY(!error.isEmpty());
    }
};
QTEST_MAIN(AnnotationTests)
#include "annotation_tests.moc"
