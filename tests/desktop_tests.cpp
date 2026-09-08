#include <QtTest>
#include <QPlainTextEdit>
#include <QClipboard>
#include <QApplication>
#include <QMimeData>
#include <QMouseEvent>
#include <QFile>
#include <QProcess>
#include <QScopeGuard>
#include <QJsonDocument>
#include <QJsonObject>
#include <QDir>
#include <Windows.h>
#include <wtsapi32.h>
#include "platform/input_service.h"
#include "platform/windows_injection.h"
#include "ui/wheel_window.h"
using namespace wheel;
class DesktopTests : public QObject {
    Q_OBJECT
    static Config shortcutConfig() {
        auto c=wheel::defaultConfig();
        c.slots[0]={"Copy",Shortcut{Qt::Key_C,bit(Modifier::Control)}};
        return c;
    }
    static Config combinationConfig() {
        auto c = shortcutConfig(); c.modifier = Modifier::Control; c.button = MouseButton::Right;
        return c;
    }
    std::unique_ptr<InputService> input_;
    std::unique_ptr<WheelWindow> wheel_;
    QPlainTextEdit editor_;
    POINT original_{};
    std::unique_ptr<QMimeData> clipboard_;
    Geometry geometry_;
    quint64 shown_ = 0;
    int rightDown_ = 0, rightUp_ = 0, middleDown_ = 0, middleUp_ = 0;
    QHash<quint64,qint64> onset_;
    QList<double> latency_;
    bool eventFilter(QObject* object, QEvent* event) override {
        if (event->type()==QEvent::MouseButtonPress || event->type()==QEvent::MouseButtonRelease) {
            auto* mouseEvent = static_cast<QMouseEvent*>(event);
            if (mouseEvent->button()==Qt::MiddleButton) {
                if (event->type()==QEvent::MouseButtonPress) ++middleDown_; else ++middleUp_;
            }
            if (mouseEvent->button()==Qt::RightButton) {
                if (event->type()==QEvent::MouseButtonPress) ++rightDown_; else ++rightUp_;
            }
        }
        return QObject::eventFilter(object,event);
    }
    static void key(WORD vk, bool down) {
        auto event = win::keyEvent(vk,down);
        event.ki.dwExtraInfo = 0x54455354;
        QCOMPARE(SendInput(1,&event,sizeof(INPUT)),UINT(1));
        QTest::qWait(12);
    }
    static void mouse(bool down, bool middle=false) {
        INPUT event{}; event.type = INPUT_MOUSE;
        event.mi.dwFlags = middle ? (down ? MOUSEEVENTF_MIDDLEDOWN : MOUSEEVENTF_MIDDLEUP) :
                                  (down ? MOUSEEVENTF_RIGHTDOWN : MOUSEEVENTF_RIGHTUP);
        event.mi.dwExtraInfo = 0x54455354;
        QCOMPARE(SendInput(1,&event,sizeof(INPUT)),UINT(1));
        QTest::qWait(12);
    }
    void activateEditor() {
        editor_.show(); editor_.raise(); editor_.activateWindow(); editor_.setFocus();
        SetForegroundWindow(reinterpret_cast<HWND>(editor_.winId()));
        editor_.setPlainText("MouseWheel desktop test"); editor_.selectAll();
        const auto scale = editor_.devicePixelRatioF();
        RECT native{}; GetWindowRect(reinterpret_cast<HWND>(editor_.winId()),&native);
        SetCursorPos(native.left+qRound(editor_.width()*scale/2),native.top+qRound(editor_.height()*scale/2));
        SetWindowPos(reinterpret_cast<HWND>(editor_.winId()),HWND_TOPMOST,0,0,0,0,
                     SWP_NOMOVE|SWP_NOSIZE|SWP_NOACTIVATE);
        INPUT clicks[2]{};
        clicks[0].type=clicks[1].type=INPUT_MOUSE;
        clicks[0].mi.dwFlags=MOUSEEVENTF_LEFTDOWN; clicks[1].mi.dwFlags=MOUSEEVENTF_LEFTUP;
        QCOMPARE(SendInput(2,clicks,sizeof(INPUT)),UINT(2));
        QTRY_COMPARE(GetForegroundWindow(),reinterpret_cast<HWND>(editor_.winId()));
        QTest::qWait(70); editor_.selectAll();
    }
    void begin(WORD modifier=VK_LCONTROL) {
        const auto previous=shown_;
        key(modifier,true); mouse(true);
        QTRY_VERIFY(shown_>previous);
    }
    void chooseCopy() {
        SetCursorPos(qRound(geometry_.center.x()),qRound(geometry_.center.y()-geometry_.radius*0.65));
        mouse(false);
    }
private Q_SLOTS:
    void initTestCase() {
        GetCursorPos(&original_);
        clipboard_ = std::make_unique<QMimeData>();
        const auto* original = QApplication::clipboard()->mimeData();
        for (const auto& format : original->formats()) clipboard_->setData(format,original->data(format));
        editor_.setWindowTitle("MouseWheel controlled input test");
        editor_.setContextMenuPolicy(Qt::NoContextMenu);
        editor_.viewport()->installEventFilter(this);
        editor_.resize(720,480);
        input_ = std::make_unique<InputService>();
        wheel_ = std::make_unique<WheelWindow>();
        connect(input_.get(),&InputService::showWheel,wheel_.get(),&WheelWindow::present);
        connect(input_.get(),&InputService::showWheel,this,[this](quint64 id,Config,Geometry g,QString){
            shown_=id; geometry_=g;
        });
        connect(input_.get(),&InputService::triggered,this,[this](quint64 id,qint64 time){ onset_[id]=time; });
        connect(wheel_.get(),&WheelWindow::firstPaint,this,[this](quint64 id,qint64 time){
            if (onset_.contains(id)) latency_.append((time-onset_.take(id))/1000000.0);
        });
        connect(input_.get(),&InputService::selection,wheel_.get(),&WheelWindow::select);
        connect(input_.get(),&InputService::hideWheel,wheel_.get(),&WheelWindow::dismiss);
        connect(wheel_.get(),&WheelWindow::hidden,input_.get(),&InputService::hidden);
        QSignalSpy ready(input_.get(),&InputService::listening);
        input_->start(combinationConfig());
        QTRY_VERIFY(!ready.empty()); QVERIFY(ready.last()[0].toBool());
        activateEditor();
    }
    void toolDispatchAfterHide_data() {
        QTest::addColumn<int>("actionKind");
        QTest::newRow("pin")<<1; QTest::newRow("screen annotation")<<2;
        QTest::newRow("application")<<3; QTest::newRow("website")<<4;
    }
    void toolDispatchAfterHide() {
        QFETCH(int,actionKind);
        activateEditor(); if(QTest::currentTestFailed()) return;
        auto config=shortcutConfig(); config.slots[0]={QStringLiteral("工具"),actionKind==1?Action{ScreenshotAction{}}:Action{AnnotationAction{}}};
        if(actionKind==int(ActionKind::Application)) config.slots[0]=Slot{"App",ApplicationAction{"C:/Program Files/应用/test.exe"}};
        if(actionKind==int(ActionKind::Website)) config.slots[0]=Slot{"Web",WebsiteAction{"https://example.com/path?a=1&b=2"}};
        input_->configure(config); QTest::qWait(40);
        QSignalSpy screenshot(input_.get(),&InputService::actionRequested);
        QApplication::clipboard()->setText("unchanged");
        const auto previous=shown_; mouse(true,true); QTRY_VERIFY(shown_>previous);
        QCOMPARE(screenshot.size(),0);
        SetCursorPos(qRound(geometry_.center.x()),qRound(geometry_.center.y()-geometry_.radius*0.65));
        mouse(false,true); QTRY_COMPARE(screenshot.size(),1);
        QCOMPARE(screenshot.first().first().value<Slot>().kind(),static_cast<ActionKind>(actionKind));
        QCOMPARE(screenshot.first().first().value<Slot>(),config.slots[0]);
        QVERIFY(!wheel_->isVisible()); QCOMPARE(QApplication::clipboard()->text(),QString("unchanged"));
    }
    void applicationExclusionPreservesInputPairs() {
        activateEditor(); auto config=shortcutConfig();
        config.triggerRules.excludedApplications={QCoreApplication::applicationFilePath()};
        input_->configure(config); QTest::qWait(60);
        const auto down=middleDown_,up=middleUp_; const auto previous=shown_;
        mouse(true,true); QCOMPARE(shown_,previous);
        input_->configure(shortcutConfig()); QTest::qWait(40); mouse(false,true);
        QTRY_COMPARE(middleDown_,down+1); QTRY_COMPARE(middleUp_,up+1);
        mouse(true,true); QTRY_VERIFY(shown_>previous);
        input_->configure(config); QTRY_VERIFY(!wheel_->isVisible()); mouse(false,true);
        QCOMPARE(middleDown_,down+1); QCOMPARE(middleUp_,up+1);
        input_->configure(shortcutConfig()); QTest::qWait(40);
        const auto resumed=shown_; mouse(true,true); QTRY_VERIFY(shown_>resumed); mouse(false,true);
    }
    void fullscreenPauseAndResume() {
        activateEditor(); auto config=shortcutConfig(); config.triggerRules.pauseFullscreen=true;
        input_->configure(config); QTest::qWait(40);
        const auto restore=qScopeGuard([&]{editor_.showNormal();});
        auto previous=shown_; mouse(true,true); QTRY_VERIFY(shown_>previous);
        editor_.showFullScreen(); QTRY_VERIFY(!wheel_->isVisible()); mouse(false,true);
        activateEditor(); QTest::qWait(80);
        previous=shown_; const auto down=middleDown_,up=middleUp_;
        mouse(true,true); mouse(false,true); QCOMPARE(shown_,previous);
        QTRY_COMPARE(middleDown_,down+1); QTRY_COMPARE(middleUp_,up+1);
        editor_.showNormal(); activateEditor(); QTest::qWait(80);
        mouse(true,true); QTRY_VERIFY(shown_>previous); mouse(false,true);
    }
    void middleHoldCopiesAndCancels() {
        activateEditor(); if (QTest::currentTestFailed()) return;
        input_->configure(shortcutConfig()); QTest::qWait(40);
        const auto beforeDown = middleDown_, beforeUp = middleUp_;
        QApplication::clipboard()->setText("before");
        auto previous = shown_; mouse(true, true); QTRY_VERIFY(shown_ > previous);
        SetCursorPos(qRound(geometry_.center.x()), qRound(geometry_.center.y()-geometry_.radius*0.65));
        mouse(false, true);
        QTRY_COMPARE(QApplication::clipboard()->text(), QString("MouseWheel desktop test"));
        QCOMPARE(GetForegroundWindow(), reinterpret_cast<HWND>(editor_.winId()));
        QCOMPARE(middleDown_, beforeDown); QCOMPARE(middleUp_, beforeUp);
        previous = shown_; mouse(true, true); QTRY_VERIFY(shown_ > previous);
        QApplication::clipboard()->setText("unchanged");
        SetCursorPos(qRound(geometry_.center.x()), qRound(geometry_.center.y()));
        mouse(false, true); QTRY_VERIFY(!wheel_->isVisible());
        QCOMPARE(QApplication::clipboard()->text(), QString("unchanged"));
        previous = shown_; mouse(true, true); QTRY_VERIFY(shown_ > previous);
        key(VK_ESCAPE,true); key(VK_ESCAPE,false); mouse(false,true);
        QCOMPARE(middleUp_,beforeUp);
        input_->pause(true); QTest::qWait(30);
        mouse(true,true); mouse(false,true);
        QTRY_COMPARE(middleDown_,beforeDown+1); QTRY_COMPARE(middleUp_,beforeUp+1);
        QVERIFY(!(GetAsyncKeyState(VK_MBUTTON)&0x8000));
    }
    void copyKeepsForegroundAndModifier() {
        activateEditor(); if (QTest::currentTestFailed()) return; QApplication::clipboard()->setText("before");
        const auto beforeDown=rightDown_, beforeUp=rightUp_;
        begin(VK_RCONTROL);
        QVERIFY(wheel_->isVisible());
        QCOMPARE(GetForegroundWindow(),reinterpret_cast<HWND>(editor_.winId()));
        chooseCopy();
        QTRY_COMPARE(QApplication::clipboard()->text(),QString("MouseWheel desktop test"));
        QVERIFY(!wheel_->isVisible());
        QVERIFY(GetAsyncKeyState(VK_RCONTROL)&0x8000);
        key(VK_RCONTROL,false);
        QCOMPARE(rightDown_,beforeDown); QCOMPARE(rightUp_,beforeUp);
    }
    void normalClicksPassThrough() {
        activateEditor();
        const auto beforeDown=rightDown_, beforeUp=rightUp_;
        mouse(true); mouse(false);
        QTRY_COMPARE(rightDown_,beforeDown+1); QTRY_COMPARE(rightUp_,beforeUp+1);
        QVERIFY(!wheel_->isVisible());
    }
    void leftClickDragAndWheelPassThrough() {
        activateEditor();
        QSignalSpy textChanges(&editor_,&QPlainTextEdit::textChanged);
        POINT point{}; GetCursorPos(&point);
        INPUT events[2]{}; events[0].type=events[1].type=INPUT_MOUSE;
        events[0].mi.dwFlags=MOUSEEVENTF_LEFTDOWN; events[1].mi.dwFlags=MOUSEEVENTF_LEFTUP;
        QCOMPARE(SendInput(2,events,sizeof(INPUT)),UINT(2));
        QTest::qWait(20);
        QCOMPARE(SendInput(2,events,sizeof(INPUT)),UINT(2));
        QTest::qWait(20);
        QCOMPARE(SendInput(1,events,sizeof(INPUT)),UINT(1));
        SetCursorPos(point.x+50,point.y);
        QTest::qWait(20);
        QCOMPARE(SendInput(1,events+1,sizeof(INPUT)),UINT(1));
        INPUT scroll{}; scroll.type=INPUT_MOUSE; scroll.mi.dwFlags=MOUSEEVENTF_WHEEL; scroll.mi.mouseData=WHEEL_DELTA;
        QCOMPARE(SendInput(1,&scroll,sizeof(INPUT)),UINT(1));
        QTest::qWait(30);
        QVERIFY(!wheel_->isVisible());
        QCOMPARE(textChanges.count(),0);
        QVERIFY(!(GetAsyncKeyState(VK_LBUTTON)&0x8000));
    }

    void escapeCancelsWithoutAction() {
        activateEditor(); if (QTest::currentTestFailed()) return; QApplication::clipboard()->setText("unchanged");
        const auto beforeUp=rightUp_;
        begin();
        key(VK_ESCAPE,true); key(VK_ESCAPE,true); key(VK_ESCAPE,false);
        chooseCopy(); key(VK_LCONTROL,false);
        QTRY_VERIFY(!wheel_->isVisible());
        QCOMPARE(QApplication::clipboard()->text(),QString("unchanged"));
        QCOMPARE(rightUp_,beforeUp);
    }
    void pauseCancelsAndNextSessionWorks() {
        activateEditor(); const auto beforeUp=rightUp_;
        begin(); input_->pause(true); QTest::qWait(40);
        mouse(false); key(VK_LCONTROL,false);
        QVERIFY(!wheel_->isVisible()); QCOMPARE(rightUp_,beforeUp);
        input_->pause(false); QTest::qWait(40);
        copyKeepsForegroundAndModifier();
    }
    void modifierNeutralization() {
        activateEditor();
        auto config=combinationConfig(); config.modifier=Modifier::Alt;
        input_->configure(config); QTest::qWait(40);
        QApplication::clipboard()->setText("before");
        begin(VK_LMENU); chooseCopy();
        QTRY_COMPARE(QApplication::clipboard()->text(),QString("MouseWheel desktop test"));
        QVERIFY(GetAsyncKeyState(VK_LMENU)&0x8000);
        QVERIFY(!(GetAsyncKeyState(VK_LCONTROL)&0x8000));
        key(VK_LMENU,false);
        QCOMPARE(GetForegroundWindow(),reinterpret_cast<HWND>(editor_.winId()));
        input_->configure(combinationConfig()); QTest::qWait(40);
    }
    void physicalReleaseBeforeExecution() {
        activateEditor(); if (QTest::currentTestFailed()) return; QApplication::clipboard()->setText("before");
        begin(); key(VK_LCONTROL,false); chooseCopy();
        QTRY_COMPARE(QApplication::clipboard()->text(),QString("MouseWheel desktop test"));
        QVERIFY(!(GetAsyncKeyState(VK_LCONTROL)&0x8000));
    }
    void focusChangeCancels() {
        activateEditor(); if (QTest::currentTestFailed()) return; QApplication::clipboard()->setText("unchanged"); begin();
        QPlainTextEdit other; other.setWindowTitle("MouseWheel alternate target"); other.show();
        SetWindowPos(reinterpret_cast<HWND>(other.winId()),HWND_TOPMOST,0,0,0,0,SWP_NOMOVE|SWP_NOSIZE);
        SetForegroundWindow(reinterpret_cast<HWND>(other.winId())); other.setFocus();
        QTRY_COMPARE(GetForegroundWindow(),reinterpret_cast<HWND>(other.winId()));
        QTRY_VERIFY(!wheel_->isVisible());
        chooseCopy(); key(VK_LCONTROL,false);
        QCOMPARE(QApplication::clipboard()->text(),QString("unchanged"));
        other.hide();
    }
    void activeConfigurationIsSnapshot() {
        activateEditor(); if (QTest::currentTestFailed()) return; QApplication::clipboard()->setText("before"); begin();
        auto config=combinationConfig(); std::get<Shortcut>(config.slots[0].action).key=Qt::Key_X;
        input_->configure(config); QTest::qWait(40); chooseCopy(); key(VK_LCONTROL,false);
        QTRY_COMPARE(QApplication::clipboard()->text(),QString("MouseWheel desktop test"));
        QCOMPARE(editor_.toPlainText(),QString("MouseWheel desktop test"));
        input_->configure(combinationConfig()); QTest::qWait(40);
    }
    void edgeGeometryMatchesNativeWindow() {
        activateEditor();
        POINT point{}; GetCursorPos(&point);
        MONITORINFO monitor{}; monitor.cbSize=sizeof(monitor);
        QVERIFY(GetMonitorInfoW(MonitorFromPoint(point,MONITOR_DEFAULTTONEAREST),&monitor));
        SetCursorPos(monitor.rcWork.left+2,monitor.rcWork.top+2); begin();
        POINT current{}; GetCursorPos(&current);
        QCOMPARE(current.x,monitor.rcWork.left+2); QCOMPARE(current.y,monitor.rcWork.top+2);
        RECT rect{}; QVERIFY(GetWindowRect(reinterpret_cast<HWND>(wheel_->winId()),&rect));
        QCOMPARE(rect.left,qRound(geometry_.center.x()-geometry_.radius));
        QCOMPARE(rect.top,qRound(geometry_.center.y()-geometry_.radius));
        QCOMPARE(rect.right-rect.left,qRound(geometry_.radius*2));
        mouse(false); key(VK_LCONTROL,false);
    }
    void sleepWakeDoesNotOverrideLock() {
        activateEditor();
        HWND events = FindWindowW(L"MouseWheelInputEvents",L"");
        QVERIFY(events);
        begin();
        SendMessageW(events,WM_WTSSESSION_CHANGE,WTS_SESSION_LOCK,0);
        QTRY_VERIFY(!wheel_->isVisible());
        mouse(false); key(VK_LCONTROL,false);
        SendMessageW(events,WM_POWERBROADCAST,PBT_APMSUSPEND,0);
        SendMessageW(events,WM_POWERBROADCAST,PBT_APMRESUMEAUTOMATIC,0);
        const auto previous=shown_;
        key(VK_LCONTROL,true); mouse(true); QTest::qWait(30);
        QCOMPARE(shown_,previous);
        mouse(false); key(VK_LCONTROL,false);
        SendMessageW(events,WM_WTSSESSION_CHANGE,WTS_SESSION_UNLOCK,0);
        QTest::qWait(30);
        copyKeepsForegroundAndModifier();
    }

    void rapidSessions() {
        activateEditor();
        for (int i=0;i<20;++i) {
            begin();
            key(VK_ESCAPE,true); key(VK_ESCAPE,false);
            mouse(false); key(VK_LCONTROL,false);
            QVERIFY(!wheel_->isVisible());
        }
    }
    void cleanup() {
        if (!input_) return;
        mouse(false,true); mouse(false); key(VK_LCONTROL,false); key(VK_RCONTROL,false); key(VK_LMENU,false);
        input_->configure(combinationConfig()); input_->pause(false); QTest::qWait(30);
    }
    void copiesFromWindowsNotepad() {
        QProcess notepad;
        notepad.start("notepad.exe");
        QVERIFY(notepad.waitForStarted());
        struct Target { DWORD process; HWND window=nullptr; } target{static_cast<DWORD>(notepad.processId())};
        QTRY_VERIFY_WITH_TIMEOUT(([&] {
            EnumWindows([](HWND hwnd,LPARAM param)->BOOL {
                auto* target=reinterpret_cast<Target*>(param);
                DWORD pid=0; GetWindowThreadProcessId(hwnd,&pid);
                if (pid==target->process && IsWindowVisible(hwnd)) { target->window=hwnd; return FALSE; }
                return TRUE;
            },reinterpret_cast<LPARAM>(&target));
            return target.window != nullptr;
        })(),3000);
        HWND edit=FindWindowExW(target.window,nullptr,L"Edit",nullptr);
        auto close=qScopeGuard([&] {
            if (edit) SendMessageW(edit,EM_SETMODIFY,FALSE,0);
            if (target.window) PostMessageW(target.window,WM_CLOSE,0,0);
            notepad.waitForFinished(2000);
        });
        QVERIFY(edit);
        SetWindowPos(target.window,HWND_TOPMOST,100,100,720,480,SWP_SHOWWINDOW);
        RECT rect{}; GetWindowRect(edit,&rect); SetCursorPos((rect.left+rect.right)/2,(rect.top+rect.bottom)/2);
        INPUT clicks[2]{}; clicks[0].type=clicks[1].type=INPUT_MOUSE;
        clicks[0].mi.dwFlags=MOUSEEVENTF_LEFTDOWN; clicks[1].mi.dwFlags=MOUSEEVENTF_LEFTUP;
        QCOMPARE(SendInput(2,clicks,sizeof(INPUT)),UINT(2));
        QTest::qWait(70);
        QTRY_COMPARE(GetForegroundWindow(),target.window);
        SendMessageW(edit,WM_SETTEXT,0,reinterpret_cast<LPARAM>(L"MouseWheel native Notepad test"));
        SendMessageW(edit,EM_SETSEL,0,-1);
        QApplication::clipboard()->setText("before");
        begin(); chooseCopy();
        QTRY_COMPARE(QApplication::clipboard()->text(),QString("MouseWheel native Notepad test"));
        QCOMPARE(GetForegroundWindow(),target.window);
        key(VK_LCONTROL,false);
    }

    void cleanupTestCase() {
        input_.reset(); wheel_.reset(); editor_.hide();
        SetCursorPos(original_.x,original_.y);
        QApplication::clipboard()->setMimeData(clipboard_.release());
        std::sort(latency_.begin(),latency_.end());
        QVERIFY(!latency_.isEmpty());
        QJsonObject report{{"samples",latency_.size()},
                           {"trigger_to_paint_p50_ms",latency_[latency_.size()/2]},
                           {"trigger_to_paint_p95_ms",latency_[qMin(latency_.size()-1,latency_.size()*95/100)]}};
        QDir().mkpath("artifacts"); QFile output("artifacts/latency.json");
        QVERIFY(output.open(QIODevice::WriteOnly)); output.write(QJsonDocument(report).toJson());
        qInfo().noquote() << QJsonDocument(report).toJson(QJsonDocument::Compact);
    }
};
QTEST_MAIN(DesktopTests)
#include "desktop_tests.moc"
