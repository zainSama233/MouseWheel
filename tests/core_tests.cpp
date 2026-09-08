#include "core/screen_helper.h"
#include <QtTest>
#include "core/interaction.h"
using namespace wheel;
class CoreTests : public QObject {
    Q_OBJECT
    static Config combinationConfig() {
        auto c = defaultConfig(); c.slots[0]={"Copy",Shortcut{Qt::Key_C,bit(Modifier::Control)}}; c.slots[2]={"Undo",Shortcut{Qt::Key_Z,bit(Modifier::Control)}}; c.modifier = Modifier::Control; c.button = MouseButton::Right;
        return c;
    }
private Q_SLOTS:
    void rootCenterAndChildReturn() {
        Config c=defaultConfig();c.centerEnabled=true;c.center={"Center",ScreenshotAction{}};c.deadZone=20;
        Geometry g{{300,300},328,164};Interaction core;
        core.press(MouseButton::Middle,0,c,g,1);
        QVERIFY(core.release(MouseButton::Middle,g.center+QPointF(0,39)).action);
        core.press(MouseButton::Middle,0,c,g,1);
        QVERIFY(!core.release(MouseButton::Middle,g.center+QPointF(0,41)).action);
        GroupAction group;group.slots[0]={"Child",ScreenshotAction{}};c.slots[0]={"Tools",group};
        core.press(MouseButton::Middle,0,c,g,1);core.move(g.center+slotCenter(0)*2,100);core.advance(450);
        QVERIFY(!core.release(MouseButton::Middle,g.center).action);
        core.press(MouseButton::Middle,0,c,g,1);core.move(g.center+slotCenter(0)*2,100);core.advance(450);
        QVERIFY(core.move(g.center,451).levelChanged);
        QVERIFY(!core.release(MouseButton::Middle,g.center).action);
    }
    void safeMarginsAndCoordinateRoundTrip() {
        const QRectF area(-1920,-1080,1920,1080);
        for(auto policy:{EdgePolicy::Translate,EdgePolicy::Shrink}) {
            const auto g=ScreenHelper::fit({-1919,-1079},area,1.5,220,{12,24},policy);
            QVERIFY(area.adjusted(18,36,-18,-36).contains(QRectF(g.center-QPointF(g.radius,g.radius),QSizeF(g.radius*2,g.radius*2))));
            const QPointF local(45,-100);QVERIFY(QLineF(ScreenHelper::toLocal(ScreenHelper::toNative(local,g),g),local).length()<.0001);
        }
    }
    void edgeLayoutsRemainOnScreen() {
        for(int count:{4,8,12}) for(auto shape:{WheelShape::Original,WheelShape::Circle,WheelShape::Capsule,WheelShape::HexagonHive})
            for(double scale:{1.,1.5,2.}) for(const QPointF point:{QPointF(-1919,1),QPointF(-1,1079)}) {
                const QRectF screen(-1920,0,1920,1080);const auto g=ScreenHelper::fit(point,screen,scale);
                QVERIFY(screen.contains(QRectF(g.center-QPointF(g.radius,g.radius),QSizeF(g.radius*2,g.radius*2))));
                for(int i=0;i<count;++i) QCOMPARE(g.hit(g.center+slotCenter(i,count,shape)*(g.radius/WheelRadius),shape,count),i);
            }
    }
    void variableLayoutsAndGroups() {
        for(int count:{4,8,12}) for(auto shape:{WheelShape::Original,WheelShape::Circle,WheelShape::Capsule,WheelShape::HexagonHive}) {
            Config c=defaultConfig(); c.slots.resize(count); c.shape=shape; QVERIFY(validate(c).isEmpty());
            Geometry g{{300,300}};
            for(int i=0;i<count;++i) {
                const auto point=slotCenter(i,count,shape);
                QCOMPARE(g.hit(g.center+point,shape,count),i);
                for(int j=0;j<count;++j) QCOMPARE(slotPath(shape,j,count).contains(point),i==j);
                QVERIFY(!slotPath(shape,i,count).contains(QPointF{}));
            }
        }
        Config c=defaultConfig();GroupAction group;group.slots[0]={"Child",ScreenshotAction{}};c.slots[0]={"Tools",group};
        Geometry g{{300,300}};Interaction core;core.press(MouseButton::Middle,0,c,g,1);
        core.move(g.center+slotCenter(0),100);
        QVERIFY(!core.advance(449).levelChanged);
        QVERIFY(core.advance(450).levelChanged);QCOMPARE(core.groupIndex(),0);
        QVERIFY(!core.release(MouseButton::Middle,g.center+slotCenter(0)).action);
        core.press(MouseButton::Middle,0,c,g,1);core.move(g.center+slotCenter(0),100);core.advance(450);
        core.move(g.center+slotCenter(0)+QPointF(15,0),451);
        QVERIFY(core.release(MouseButton::Middle,g.center+slotCenter(0)).action);
        core.press(MouseButton::Middle,0,c,g,1);core.move(g.center+slotCenter(0),100);core.advance(450);
        QVERIFY(core.move(g.center,451).levelChanged);QCOMPARE(core.groupIndex(),-1);
        QVERIFY(!core.release(MouseButton::Middle,g.center).action);
        c.slots[0]=Slot{"Nested",GroupAction{}};
        std::get<GroupAction>(c.slots[0].action).slots[0]=Slot{"Nested",GroupAction{}};
        QVERIFY(!validate(c).isEmpty());
    }

    void contextPermissionPreservesPairs() {
        auto c=defaultConfig(); Interaction core; Geometry g{{400,400}};
        QVERIFY(!core.press(MouseButton::Middle,0,c,g,1,false).consumed);
        QVERIFY(!core.release(MouseButton::Middle,g.center).consumed);
        QVERIFY(core.press(MouseButton::Middle,0,c,g,1,true).show);
        QVERIFY(core.press(MouseButton::Middle,0,c,g,1,false).consumed);
        core.cancel(); QVERIFY(core.release(MouseButton::Middle,g.center).consumed);
        QVERIFY(!core.press(MouseButton::Middle,0,c,g,1,false).show);
        QVERIFY(!core.release(MouseButton::Middle,g.center).consumed);
        QVERIFY(core.press(MouseButton::Middle,0,c,g,1,true).show);
    }
    void triggerRulesMatchExecutableIdentity() {
        TriggerRules rules; rules.excludedApplications={"C:/Apps/Editor.exe"};
        QVERIFY(!rules.allows("c:\\apps\\EDITOR.EXE",false));
        QVERIFY(rules.allows("D:/Apps/Editor.exe",false));
        QVERIFY(rules.allows("C:/Apps/Other.exe",true));
        rules.pauseFullscreen=true;
        QVERIFY(!rules.allows("C:/Apps/Other.exe",true));
        QVERIFY(rules.allows("C:/Apps/Other.exe",false));
    }
    void shapedHitRegions() {
        for(auto shape:{WheelShape::Original,WheelShape::Circle,WheelShape::HexagonHive}) {
            auto geometry=ScreenHelper::fit({400,400},{0,0,1000,1000},1);
            for(int index=0;index<8;++index) {
                QCOMPARE(geometry.hit(geometry.center+slotCenter(index,8,shape),shape),index);
                QVERIFY(slotPath(shape,index).contains(slotCenter(index,8,shape)));
            }
            QCOMPARE(geometry.hit(geometry.center+QPointF(0,-54),shape),-1);
            QCOMPARE(geometry.hit(geometry.center+QPointF(0,-163),shape),-1);
            auto config=defaultConfig(); config.shape=shape;
            Interaction core; QVERIFY(core.press(MouseButton::Middle,0,config,geometry,1).show);
            QVERIFY(!core.release(MouseButton::Middle,geometry.center+QPointF(0,-54)).action);
        }
    }
    void launcherValidation() {
        auto config=defaultConfig();
        config.slots[0]={QStringLiteral("网页"),WebsiteAction{"https://example.com/a?q=test"}};
        QVERIFY(validate(config).isEmpty());
        for(auto target:{"javascript:alert(1)","https://","file:///C:/temp/a","https://example.com/a b"}) {
            std::get<WebsiteAction>(config.slots[0].action).url=target; QVERIFY(!validate(config).isEmpty());
        }
        config.slots[0]={QStringLiteral("应用"),ApplicationAction{"C:/Program Files/App/app.exe"}};
        QVERIFY(validate(config).isEmpty());
        std::get<ApplicationAction>(config.slots[0].action).path="relative.exe"; QVERIFY(!validate(config).isEmpty());
    }
    void screenshotIsExecutableWithoutShortcut() {
        auto config=defaultConfig();
        config.slots[0]={QStringLiteral("截图"),ScreenshotAction{}};
        QVERIFY(validate(config).isEmpty());
        Interaction core; auto g=ScreenHelper::fit({500,500},{0,0,1000,1000},1);
        QVERIFY(core.press(MouseButton::Middle,0,config,g,1).show);
        auto result=core.release(MouseButton::Middle,g.center+QPointF(0,-100));
        QVERIFY(result.action); QCOMPARE(result.action->kind(),ActionKind::Screenshot);
    }
    void middleHoldDefaultAndPairing() {
        const auto c = defaultConfig();
        QCOMPARE(static_cast<unsigned>(c.modifier), 0u);
        QCOMPARE(c.button, MouseButton::Middle);
        QVERIFY(validate(c).isEmpty());
        Interaction core;
        auto g = ScreenHelper::fit({500,500}, {0,0,1920,1080}, 1);
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
        auto g = ScreenHelper::fit({2, 2}, {0, 0, 1920, 1080}, 1.5);
        QCOMPARE(g.center, QPointF(246, 246));
        QCOMPARE(g.hit(g.center), -1);
        QCOMPARE(g.hit(g.center + QPointF(0, -160)), 0);
        QCOMPARE(g.hit(g.center + QPointF(160, 0)), 2);
        QCOMPARE(g.hit(g.center + QPointF(0, 160)), 4);
        QCOMPARE(g.hit(g.center + QPointF(-160, 0)), 6);
        QCOMPARE(g.hit(g.center + QPointF(1000, 0)), -1);
        auto tiny = ScreenHelper::fit({0, 0}, {-100, -100, 120, 120}, 2);
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
        auto g = ScreenHelper::fit({500,500}, {0,0,1920,1080}, 1);
        auto start = core.press(MouseButton::Right, bit(Modifier::Control), c, g, 123);
        QVERIFY(start.show);
        const auto id = start.session;
        c.slots[2].name = "changed";
        core.move(g.center + QPointF(0,-100));
        auto end = core.release(MouseButton::Right, g.center + QPointF(100,0));
        QVERIFY(end.consumed);
        QVERIFY(end.hide);
        QVERIFY(end.action.has_value());
        QCOMPARE(std::get<Shortcut>(end.action->action).key, std::get<Shortcut>(combinationConfig().slots[2].action).key);
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
        auto g = ScreenHelper::fit({500,500}, {0,0,1000,1000}, 1);
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
