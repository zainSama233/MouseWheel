#pragma once
#include <QApplication>
#include <QMouseEvent>
#include <QScreen>
#include <QTimer>
#include <QtTest>
#include <Windows.h>
#include "ui/wheel_window.h"

enum class MouseInput { RightDown, RightUp, LeftDown, LeftUp };

class ChordProbe final : public QWidget {
public:
    explicit ChordProbe(bool defer) : defer_(defer) {
        setWindowTitle(QStringLiteral("MouseWheel · controlled chord probe"));
        resize(720, 480);
    }
    ~ChordProbe() override {
        if (hook_) UnhookWindowsHookEx(hook_);
        current_ = nullptr;
        wheel_.hide();
        INPUT events[2]{};
        events[0].type = events[1].type = INPUT_MOUSE;
        events[0].mi.dwFlags = MOUSEEVENTF_RIGHTUP;
        events[1].mi.dwFlags = MOUSEEVENTF_LEFTUP;
        events[0].mi.dwExtraInfo = events[1].mi.dwExtraInfo = replayTag;
        SendInput(2, events, sizeof(INPUT));
        hide();
        SetCursorPos(original_.x, original_.y);
        if (previousForeground_ && IsWindow(previousForeground_)) SetForegroundWindow(previousForeground_);
    }
    bool start() {
        GetCursorPos(&original_);
        previousForeground_ = GetForegroundWindow();
        show();
        const auto hwnd = reinterpret_cast<HWND>(winId());
        SetWindowPos(hwnd, HWND_TOPMOST, 200, 200, 720, 480, SWP_SHOWWINDOW);
        SetForegroundWindow(hwnd);
        QTest::qWait(100);
        RECT rect{};
        if (!GetWindowRect(hwnd, &rect)) return false;
        SetCursorPos((rect.left + rect.right) / 2, (rect.top + rect.bottom) / 2);
        if (GetForegroundWindow() != hwnd) return false;
        current_ = this;
        hook_ = SetWindowsHookExW(WH_MOUSE_LL, mouseProc, GetModuleHandleW(nullptr), 0);
        return hook_ != nullptr;
    }
    bool send(MouseInput input) {
        if (GetForegroundWindow() != reinterpret_cast<HWND>(winId())) return false;
        INPUT event{};
        event.type = INPUT_MOUSE;
        switch (input) {
        case MouseInput::RightDown: event.mi.dwFlags = MOUSEEVENTF_RIGHTDOWN; break;
        case MouseInput::RightUp: event.mi.dwFlags = MOUSEEVENTF_RIGHTUP; break;
        case MouseInput::LeftDown: event.mi.dwFlags = MOUSEEVENTF_LEFTDOWN; break;
        case MouseInput::LeftUp: event.mi.dwFlags = MOUSEEVENTF_LEFTUP; break;
        }
        event.mi.dwExtraInfo = testTag;
        const bool ok = SendInput(1, &event, sizeof(INPUT)) == 1;
        QTest::qWait(20);
        return ok;
    }
    bool moveBy(int x, int y) {
        if (GetForegroundWindow() != reinterpret_cast<HWND>(winId())) return false;
        POINT point{};
        return GetCursorPos(&point) && SetCursorPos(point.x + x, point.y + y);
    }
    int rightDowns() const { return rightDowns_; }
    int rightUps() const { return rightUps_; }
    int leftDowns() const { return leftDowns_; }
    int leftUps() const { return leftUps_; }
    int rightDragMoves() const { return rightDragMoves_; }
    bool wheelVisible() const { return wheel_.isVisible(); }
protected:
    void mousePressEvent(QMouseEvent* event) override {
        if (event->button() == Qt::RightButton) ++rightDowns_;
        if (event->button() == Qt::LeftButton) ++leftDowns_;
    }
    void mouseReleaseEvent(QMouseEvent* event) override {
        if (event->button() == Qt::RightButton) ++rightUps_;
        if (event->button() == Qt::LeftButton) ++leftUps_;
    }
    void mouseMoveEvent(QMouseEvent* event) override {
        if (event->buttons() & Qt::RightButton) ++rightDragMoves_;
    }
private:
    static LRESULT CALLBACK mouseProc(int code, WPARAM message, LPARAM data) {
        if (code < 0 || !current_) return CallNextHookEx(nullptr, code, message, data);
        const auto& event = *reinterpret_cast<MSLLHOOKSTRUCT*>(data);
        auto& probe = *current_;
        if (event.dwExtraInfo != testTag ||
            GetForegroundWindow() != reinterpret_cast<HWND>(probe.winId()))
            return CallNextHookEx(nullptr, code, message, data);
        if (message == WM_RBUTTONDOWN) {
            probe.rightHeld_ = true;
            if (probe.defer_) return 1;
        } else if (message == WM_LBUTTONDOWN && probe.rightHeld_) {
            probe.chord_ = true;
            probe.leftConsumed_ = true;
            MONITORINFO info{};
            info.cbSize = sizeof(info);
            if (GetMonitorInfoW(MonitorFromPoint(event.pt, MONITOR_DEFAULTTONEAREST), &info)) {
                const auto& r = info.rcWork;
                const auto geometry = wheel::Geometry::fit({double(event.pt.x), double(event.pt.y)},
                    {double(r.left), double(r.top), double(r.right-r.left), double(r.bottom-r.top)},
                    probe.devicePixelRatioF());
                probe.wheel_.present(1, wheel::defaultConfig(), geometry, probe.screen()->name());
            }
            return 1;
        } else if (message == WM_LBUTTONUP && probe.leftConsumed_) {
            probe.leftConsumed_ = false;
            return 1;
        } else if (message == WM_RBUTTONUP && probe.rightHeld_) {
            probe.rightHeld_ = false;
            if (probe.chord_) {
                probe.chord_ = false;
                probe.wheel_.dismiss(1);
                return 1;
            }
            if (probe.defer_) {
                QTimer::singleShot(0, &probe, [] {
                    INPUT clicks[2]{};
                    clicks[0].type = clicks[1].type = INPUT_MOUSE;
                    clicks[0].mi.dwFlags = MOUSEEVENTF_RIGHTDOWN;
                    clicks[1].mi.dwFlags = MOUSEEVENTF_RIGHTUP;
                    clicks[0].mi.dwExtraInfo = clicks[1].mi.dwExtraInfo = replayTag;
                    if (SendInput(2, clicks, sizeof(INPUT)) != 2) qFatal("Probe click replay failed");
                });
                return 1;
            }
        }
        return CallNextHookEx(nullptr, code, message, data);
    }
    static constexpr ULONG_PTR testTag = 0x43485244;
    static constexpr ULONG_PTR replayTag = 0x43485250;
    inline static ChordProbe* current_ = nullptr;
    const bool defer_;
    bool rightHeld_ = false, chord_ = false, leftConsumed_ = false;
    int rightDowns_ = 0, rightUps_ = 0, leftDowns_ = 0, leftUps_ = 0, rightDragMoves_ = 0;
    HHOOK hook_ = nullptr;
    POINT original_{};
    HWND previousForeground_ = nullptr;
    wheel::WheelWindow wheel_;
};
