#include <QtTest>
#include "platform/windows_injection.h"
using namespace wheel;
using namespace wheel::win;
class InjectionTests : public QObject {
    Q_OBJECT
private Q_SLOTS:
    void retainsSidesAndRestoresExtraModifiers() {
        KeyState state{}; state[VK_RCONTROL] = true; state[VK_LSHIFT] = true;
        auto plan = injectionPlan({Qt::Key_C, bit(Modifier::Control)}, state);
        QCOMPARE(plan.size(), size_t(4));
        QCOMPARE(plan.front().ki.wVk, WORD(VK_LSHIFT));
        QVERIFY(plan.front().ki.dwFlags & KEYEVENTF_KEYUP);
        QCOMPARE(plan.back().ki.wVk, WORD(VK_LSHIFT));
        QVERIFY(!(plan.back().ki.dwFlags & KEYEVENTF_KEYUP));
        for (const auto& e : plan) {
            QCOMPARE(e.ki.dwExtraInfo, injectionTag);
            QVERIFY(e.ki.wVk != VK_LCONTROL && e.ki.wVk != VK_RCONTROL);
        }
    }
    void partialFailureCleansOnlyOwnedKeys() {
        KeyState state{};
        const auto plan = injectionPlan({Qt::Key_C, bit(Modifier::Control)}, state);
        auto cleanup = recoveryPlan(plan, 2, state);
        QCOMPARE(cleanup.size(), size_t(2));
        for (const auto& e : cleanup) QVERIFY(e.ki.dwFlags & KEYEVENTF_KEYUP);
        QVERIFY(recoveryPlan(plan, 0, state).empty());
        QVERIFY(recoveryPlan(plan, plan.size(), state).empty());
    }
    void everySupportedKeyMaps() {
        for (int key=Qt::Key_A; key<=Qt::Key_Z; ++key) QVERIFY(virtualKey(key));
        for (int key=Qt::Key_F1; key<=Qt::Key_F24; ++key) QVERIFY(virtualKey(key));
        QVERIFY(!virtualKey(Qt::Key_Control));
    }
    void heldActionKeyCannotBeReleasedByUs() {
        KeyState state{}; state['C'] = true;
        QVERIFY(injectionPlan({Qt::Key_C, bit(Modifier::Control)}, state).empty());
    }
};
QTEST_GUILESS_MAIN(InjectionTests)
#include "injection_tests.moc"
