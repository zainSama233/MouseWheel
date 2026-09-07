#include <QtTest>
#include <QApplication>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QDir>
#include "support/chord_probe.h"

class ChordProbeTests final : public QObject {
    Q_OBJECT
    QJsonArray observations_;
private Q_SLOTS:
    void characterize_data() {
        QTest::addColumn<bool>("defer");
        QTest::newRow("immediate-right-down") << false;
        QTest::newRow("deferred-right-down") << true;
    }
    void characterize() {
        QFETCH(bool, defer);
        ChordProbe probe(defer);
        QVERIFY2(probe.start(), "Cannot activate the controlled window or install the native hook");
        QVERIFY(probe.send(MouseInput::RightDown));
        QTest::qWait(60);
        const int normalBeforeRelease = probe.rightDowns();
        QVERIFY(probe.send(MouseInput::RightUp));
        QTRY_COMPARE(probe.rightUps(), 1);
        QCOMPARE(probe.rightDowns(), 1);

        const int beforeChord = probe.rightDowns();
        QVERIFY(probe.send(MouseInput::RightDown));
        QVERIFY(probe.send(MouseInput::LeftDown));
        QTRY_VERIFY(probe.wheelVisible());
        const int chordDowns = probe.rightDowns() - beforeChord;
        QVERIFY(probe.send(MouseInput::LeftUp));
        QVERIFY(probe.send(MouseInput::RightUp));
        QTRY_VERIFY(!probe.wheelVisible());
        const int chordUps = probe.rightUps() - 1;

        const int beforeDrag = probe.rightDowns();
        QVERIFY(probe.send(MouseInput::RightDown));
        QVERIFY(probe.moveBy(50, 0));
        QTest::qWait(60);
        const int dragDowns = probe.rightDowns() - beforeDrag;
        const int dragMoves = probe.rightDragMoves();
        QVERIFY(probe.send(MouseInput::RightUp));
        QTest::qWait(60);

        QCOMPARE(normalBeforeRelease, defer ? 0 : 1);
        QCOMPARE(chordDowns, defer ? 0 : 1);
        QCOMPARE(chordUps, 0);
        QCOMPARE(dragDowns, defer ? 0 : 1);
        if (defer) QCOMPARE(dragMoves, 0);
        else QVERIFY(dragMoves > 0);
        QCOMPARE(probe.leftDowns(), 0);
        QCOMPARE(probe.leftUps(), 0);
        observations_.append(QJsonObject{
            {"strategy", defer ? "deferred" : "immediate"},
            {"normal_right_down_before_release", normalBeforeRelease},
            {"chord_right_down_delivered", chordDowns},
            {"chord_right_up_delivered", chordUps},
            {"right_down_delivered_before_drag_release", dragDowns},
            {"native_right_drag_moves", dragMoves},
            {"product_acceptance", false}
        });
    }
    void cleanupTestCase() {
        QDir().mkpath("artifacts");
        QFile file("artifacts/chord-probe.json");
        QVERIFY(file.open(QIODevice::WriteOnly));
        const auto data = QJsonDocument(observations_).toJson();
        QCOMPARE(file.write(data), data.size());
        qInfo().noquote() << data;
    }
};
QTEST_MAIN(ChordProbeTests)
#include "chord_probe_tests.moc"
