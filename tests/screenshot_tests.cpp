#include <QtTest>
#include <QApplication>
#include <QClipboard>
#include <QMimeData>
#include <QPointer>
#include <QAction>
#include <QScreen>
#include <QDir>
#include <QWheelEvent>
#include <QTemporaryDir>
#include "tools/pinned_image.h"
#include "tools/screenshot_session.h"
#include "tools/region_capture.h"
using namespace wheel;
class ScreenshotTests final : public QObject {
    Q_OBJECT
    std::unique_ptr<QMimeData> original_;
private Q_SLOTS:
    void initTestCase() {
        qRegisterMetaType<QScreen*>();
        original_=std::make_unique<QMimeData>();
        const auto* data=QApplication::clipboard()->mimeData();
        for(const auto& format:data->formats()) original_->setData(format,data->data(format));
    }
    void pinCopySaveAndZoom_data() {
        QTest::addColumn<int>("theme");
        QTest::newRow("light")<<0; QTest::newRow("warm")<<1; QTest::newRow("dark")<<2;
    }
    void pinCopySaveAndZoom() {
        QFETCH(int,theme);
        QImage image(600,400,QImage::Format_ARGB32_Premultiplied); image.fill(Qt::green);
        QPointer<PinnedImage> pin=new PinnedImage(image,static_cast<Theme>(theme));
        pin->show(); QVERIFY(QTest::qWaitForWindowExposed(pin));
        const auto size=pin->size();
        QWheelEvent wheel(QPointF(100,100),pin->mapToGlobal(QPoint(100,100)),{},QPoint(0,120),Qt::NoButton,Qt::NoModifier,Qt::NoScrollPhase,false);
        QApplication::sendEvent(pin,&wheel); QVERIFY(pin->width()>size.width()); QCOMPARE(pin->image(),image);
        const auto before=pin->pos();
        QTest::mousePress(pin,Qt::LeftButton,Qt::NoModifier,QPoint(100,100));
        QTest::mouseMove(pin,QPoint(130,120)); QTest::mouseRelease(pin,Qt::LeftButton,Qt::NoModifier,QPoint(130,120));
        QVERIFY(pin->pos()!=before);
        pin->findChild<QAction*>("copy-image")->trigger();
        QCOMPARE(QApplication::clipboard()->image(),image); QVERIFY(pin->isVisible());
        QTemporaryDir dir; QString error;
        QVERIFY(pin->savePng(dir.filePath("pin.png"),error));
        QCOMPARE(QImage(dir.filePath("pin.png")).convertToFormat(image.format()),image);
        QVERIFY(!pin->savePng(dir.filePath("missing/pin.png"),error)); QVERIFY(!error.isEmpty());
        QDir().mkpath("artifacts"); QVERIFY(pin->grab().save(QString("artifacts/pinned-image-%1.png").arg(theme)));
        pin->close(); QTRY_VERIFY(pin.isNull());
    }
    void sampleRealScreenPixelAndCancel() {
        QWidget background;background.setWindowFlags(Qt::Window|Qt::WindowStaysOnTopHint);background.setStyleSheet("background:#18764b;");background.resize(500,400);background.show();background.raise();
        QVERIFY(QTest::qWaitForWindowExposed(&background));QTest::qWait(200);const auto point=background.mapToGlobal(QPoint(140,140));
        const auto native=(point-background.screen()->geometry().topLeft())*background.screen()->devicePixelRatio();
        QTRY_COMPARE_WITH_TIMEOUT(background.screen()->grabWindow(0).toImage().pixelColor(native),QColor("#18764b"),3000);
        const QColor expected=background.screen()->grabWindow(0).toImage().pixelColor(native);
        RegionCapture capture;QSignalSpy selected(&capture,&RegionCapture::selected);QString error;QVERIFY(capture.start(Theme::Light,error,RegionCapture::Mode::Pixel));
        QWidget* picker=nullptr;for(auto* widget:QApplication::topLevelWidgets())if(widget->objectName()=="region-picker" && widget->screen()==background.screen())picker=widget;
        QVERIFY(picker);QTest::mouseClick(picker,Qt::LeftButton,Qt::NoModifier,picker->mapFromGlobal(point));QTRY_VERIFY(!capture.active());QCOMPARE(selected.size(),1);
        const auto pixel=qvariant_cast<QImage>(selected[0][0]);QCOMPARE(pixel.size(),QSize(1,1));QCOMPARE(pixel.pixelColor(0,0),expected);
        QVERIFY(capture.start(Theme::Light,error,RegionCapture::Mode::Pixel));capture.cancel();QVERIFY(!capture.active());QCOMPARE(selected.size(),1);
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
        QPointer<PinnedImage> editor;
        QTRY_VERIFY(([&]{for(auto* widget:QApplication::topLevelWidgets()) if(auto* found=qobject_cast<PinnedImage*>(widget)) editor=found; return !editor.isNull();})());
        const auto captured=editor->image();
        QCOMPARE(captured.pixelColor(captured.width()/2,captured.height()/2),expectedColor);
        const double scale=background.screen()->devicePixelRatio();
        QVERIFY(qAbs(captured.width()-qRound(160*scale))<=1);
        QVERIFY(qAbs(captured.height()-qRound(120*scale))<=1);
        QVERIFY(!session.active()); QVERIFY(editor->isVisible());
        QVERIFY(session.start(Theme::Light,error));
        for(auto* widget:QApplication::topLevelWidgets()) if(widget->objectName()=="region-picker") { QTest::keyClick(widget,Qt::Key_Escape); break; }
        QTRY_VERIFY(!session.active()); QVERIFY(editor->isVisible());
        QVERIFY(session.start(Theme::Warm,error));
        for(auto* widget:QApplication::topLevelWidgets()) if(widget->objectName()=="region-picker") {
            QTest::mousePress(widget,Qt::LeftButton,Qt::NoModifier,QPoint(100,100));
            QTest::mouseRelease(widget,Qt::LeftButton,Qt::NoModifier,QPoint(200,180)); break;
        }
        QTRY_VERIFY(!session.active());
        QList<PinnedImage*> pins;
        for(auto* widget:QApplication::topLevelWidgets()) if(auto* pin=qobject_cast<PinnedImage*>(widget)) pins.append(pin);
        QCOMPARE(pins.size(),2); QVERIFY(editor->isVisible());
        for(auto* pin:pins) if(pin!=editor.data()) pin->close();
        editor->close(); QTRY_VERIFY(editor.isNull());
    }
    void cleanupTestCase() { QApplication::clipboard()->setMimeData(original_.release()); }
};
QTEST_MAIN(ScreenshotTests)
#include "screenshot_tests.moc"
