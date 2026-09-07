#include <QtTest>
#include <QApplication>
#include <QClipboard>
#include <QMimeData>
#include <QPointer>
#include <QAction>
#include <QScreen>
#include <QDir>
#include "tools/image_editor.h"
#include "tools/screenshot_session.h"
using namespace wheel;
class ScreenshotTests final : public QObject {
    Q_OBJECT
    std::unique_ptr<QMimeData> original_;
private Q_SLOTS:
    void initTestCase() {
        original_=std::make_unique<QMimeData>();
        const auto* data=QApplication::clipboard()->mimeData();
        for(const auto& format:data->formats()) original_->setData(format,data->data(format));
    }
    void editCopyAndRelease_data() {
        QTest::addColumn<int>("theme");
        QTest::newRow("light")<<0; QTest::newRow("warm")<<1; QTest::newRow("dark")<<2;
    }
    void editCopyAndRelease() {
        QFETCH(int,theme);
        QImage image(600,400,QImage::Format_ARGB32_Premultiplied); image.fill(Qt::white);
        QPointer<ImageEditor> editor=new ImageEditor(image,static_cast<Theme>(theme));
        editor->show(); QVERIFY(QTest::qWaitForWindowExposed(editor));
        auto* canvas=editor->findChild<AnnotationCanvas*>(); QVERIFY(canvas);
        editor->findChild<QAction*>("tool-1")->trigger();
        auto area=canvas->imageRect();
        const auto start=(area.topLeft()+QPointF(40,40)).toPoint();
        const auto end=(area.center()).toPoint();
        QTest::mousePress(canvas,Qt::LeftButton,Qt::NoModifier,start);
        QTest::mouseMove(canvas,end); QTest::mouseRelease(canvas,Qt::LeftButton,Qt::NoModifier,end);
        QVERIFY(editor->document().image()!=image);
        editor->findChild<QAction*>("undo")->trigger(); QCOMPARE(editor->document().image(),image);
        editor->document().history().redo();
        const auto expected=editor->document().image();
        QDir().mkpath("artifacts"); QVERIFY(editor->grab().save(QString("artifacts/screenshot-editor-%1.png").arg(theme)));
        editor->findChild<QAction*>("copy-image")->trigger();
        QTRY_VERIFY(editor.isNull()); QCOMPARE(QApplication::clipboard()->image(),expected);
    }
    void captureRealScreenAndCancel() {
        QWidget background; background.setWindowFlags(Qt::Window|Qt::WindowStaysOnTopHint);
        background.setStyleSheet("background:#18764b;"); background.resize(500,400);
        background.show(); background.raise(); background.activateWindow();
        QVERIFY(QTest::qWaitForWindowExposed(&background)); QTest::qWait(400);
        const auto globalSample=background.mapToGlobal(QPoint(140,140));
        const auto nativeSample=(globalSample-background.screen()->geometry().topLeft())*background.screen()->devicePixelRatio();
        const auto reference=background.screen()->grabWindow(0).toImage();
        const auto expectedColor=reference.pixelColor(nativeSample);
        ScreenshotSession session; QString error; QVERIFY2(session.start(Theme::Dark,error),qPrintable(error));
        QWidget* picker=nullptr;
        for(auto* widget:QApplication::topLevelWidgets())
            if(widget->objectName()=="region-picker" && widget->screen()==background.screen()) picker=widget;
        QVERIFY(picker); QVERIFY(QTest::qWaitForWindowExposed(picker));
        background.setStyleSheet("background:#f04030;");
        const auto start=picker->mapFromGlobal(background.mapToGlobal(QPoint(60,80)));
        const auto end=start+QPoint(160,120);
        QTest::mousePress(picker,Qt::LeftButton,Qt::NoModifier,start);
        QTest::mouseMove(picker,end); QTest::mouseRelease(picker,Qt::LeftButton,Qt::NoModifier,end);
        QPointer<ImageEditor> editor;
        QTRY_VERIFY(([&]{for(auto* widget:QApplication::topLevelWidgets()) if(auto* found=qobject_cast<ImageEditor*>(widget)) editor=found; return !editor.isNull();})());
        const auto captured=editor->document().image();
        QCOMPARE(captured.pixelColor(captured.width()/2,captured.height()/2),expectedColor);
        const double scale=background.screen()->devicePixelRatio();
        QVERIFY(qAbs(captured.width()-qRound(160*scale))<=1);
        QVERIFY(qAbs(captured.height()-qRound(120*scale))<=1);
        QVERIFY(session.active()); editor->close(); QTRY_VERIFY(!session.active());
        QVERIFY(session.start(Theme::Light,error));
        for(auto* widget:QApplication::topLevelWidgets()) if(widget->objectName()=="region-picker") { QTest::keyClick(widget,Qt::Key_Escape); break; }
        QTRY_VERIFY(!session.active());
    }
    void cleanupTestCase() { QApplication::clipboard()->setMimeData(original_.release()); }
};
QTEST_MAIN(ScreenshotTests)
#include "screenshot_tests.moc"
