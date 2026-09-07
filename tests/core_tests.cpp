#include <QtTest>
#include "core/interaction.h"
using namespace wheel;
class CoreTests : public QObject {
    Q_OBJECT
    static Config combinationConfig() {
        auto c = defaultConfig(); c.modifier = Modifier::Control; c.button = MouseButton::Right;
        return c;
    }
private Q_SLOTS:
    void middleHoldDefaultAndPairing() {
        const auto c = defaultConfig();
        QCOMPARE(static_cast<unsigned>(c.modifier), 0u);
        QCOMPARE(c.button, MouseButton::Middle);
        QVERIFY(validate(c).isEmpty());
        Interaction core;
        auto g = Geometry::fit({500,500}, {0,0,1920,1080}, 1);
        QVERIFY(!core.press(MouseButton::Right, 0, c, g, 1).consumed);
        QVERIFY(core.press(MouseButton::Middle, 0, c, g, 1).show);
        QVERIFY(core.release(MouseButton::Middle, g.center + QPointF(0,-100)).action);
        QVERIFY(!core.release(MouseButton::Middle, {}).consumed);
        QVERIFY(core.press(MouseButton::Middle, 0, c, g, 1).show);
        QVERIFY(core.setPaused(true).hide);
        const auto cancelled = core.release(MouseButton::Middle, g.center + QPointF(0,-100));
        QVERIFY(cancelled.consumed); QVERIFY(!cancelled.action);
        QVERIFY(!core.press(MouseButton::Middle, 0, c, g, 1).consumed);
    }
    void geometry() {
        auto g = Geometry::fit({2, 2}, {0, 0, 1920, 1080}, 1.5);
        QCOMPARE(g.center, QPointF(246, 246));
        QCOMPARE(g.hit(g.center), -1);
        QCOMPARE(g.hit(g.center + QPointF(0, -160)), 0);
        QCOMPARE(g.hit(g.center + QPointF(160, 0)), 2);
        QCOMPARE(g.hit(g.center + QPointF(0, 160)), 4);
        QCOMPARE(g.hit(g.center + QPointF(-160, 0)), 6);
        QCOMPARE(g.hit(g.center + QPointF(1000, 0)), -1);
        auto tiny = Geometry::fit({0, 0}, {-100, -100, 120, 120}, 2);
        QVERIFY(tiny.radius <= 60);
    }
    void normalInputPasses() {
        Interaction core;
        auto c = combinationConfig();
        QVERIFY(!core.press(MouseButton::Right, 0, c, {}, 1).consumed);
        QVERIFY(!core.release(MouseButton::Right, {}).consumed);
        QVERIFY(!core.escape(true).consumed);
    }
    void finalPositionAndSnapshot() {
        Interaction core;
        auto c = combinationConfig();
        auto g = Geometry::fit({500,500}, {0,0,1920,1080}, 1);
        auto start = core.press(MouseButton::Right, bit(Modifier::Control), c, g, 123);
        QVERIFY(start.show);
        const auto id = start.session;
        c.slots[2].name = "changed";
        core.move(g.center + QPointF(0,-100));
        auto end = core.release(MouseButton::Right, g.center + QPointF(100,0));
        QVERIFY(end.consumed);
        QVERIFY(end.hide);
        QVERIFY(end.action.has_value());
        QCOMPARE(end.action->key, combinationConfig().slots[2].shortcut.key);
        QCOMPARE(end.target, quintptr(123));
        QCOMPARE(end.session, id);
    }
    void pairedCancellation() {
        Interaction core;
        const auto c = combinationConfig();
        core.press(MouseButton::Right, bit(Modifier::Control), c, {}, 1);
        QVERIFY(core.escape(true).consumed);
        QVERIFY(core.escape(true).consumed);
        QVERIFY(core.escape(false).consumed);
        QVERIFY(!core.escape(false).consumed);
        auto result = core.release(MouseButton::Right, {100,0});
        QVERIFY(result.consumed);
        QVERIFY(!result.action);
        QVERIFY(!core.release(MouseButton::Right, {}).consumed);
    }
    void pausedPairing() {
        Interaction core;
        auto c = combinationConfig();
        core.press(MouseButton::Right, bit(Modifier::Control), c, {}, 1);
        QVERIFY(core.setPaused(true).hide);
        QVERIFY(core.release(MouseButton::Right, {}).consumed);
        QVERIFY(!core.press(MouseButton::Right, bit(Modifier::Control), c, {}, 1).consumed);
    }
    void emptyAndCenterCancel() {
        auto c = combinationConfig();
        c.slots[0] = {};
        auto g = Geometry::fit({500,500}, {0,0,1000,1000}, 1);
        Interaction core;
        core.press(MouseButton::Right, bit(Modifier::Control), c, g, 1);
        QVERIFY(!core.release(MouseButton::Right, g.center + QPointF(0,-100)).action);
        core.press(MouseButton::Right, bit(Modifier::Control), c, g, 1);
        QVERIFY(!core.release(MouseButton::Right, g.center).action);
    }
    void extraModifierDoesNotTrigger() {
        Interaction core;
        QVERIFY(!core.press(MouseButton::Right, bit(Modifier::Control) | bit(Modifier::Shift),
                            combinationConfig(), {}, 1).consumed);
    }
    void repeatDoesNotStartNewSession() {
        Interaction core;
        auto first = core.press(MouseButton::Right, bit(Modifier::Control), combinationConfig(), {}, 1);
        auto repeat = core.press(MouseButton::Right, bit(Modifier::Control), combinationConfig(), {}, 1);
        QVERIFY(repeat.consumed);
        QVERIFY(!repeat.show);
        QCOMPARE(core.session(), first.session);
    }
};
QTEST_GUILESS_MAIN(CoreTests)
#include "core_tests.moc"
