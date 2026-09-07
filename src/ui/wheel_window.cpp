#include "ui/wheel_window.h"
#include "ui/theme.h"
#include "core/clock.h"
#include <QPainter>
#include <QPainterPath>
#include <QGuiApplication>
#include <QScreen>
#include <Windows.h>
#include <cmath>
#include <numbers>
namespace wheel {
WheelWindow::WheelWindow(bool overlay, QWidget* parent) : QWidget(parent), overlay_(overlay) {
    if (overlay_) {
        setWindowFlags(Qt::Tool | Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint |
                       Qt::WindowDoesNotAcceptFocus | Qt::WindowTransparentForInput);
        setAttribute(Qt::WA_TranslucentBackground);
        setAttribute(Qt::WA_ShowWithoutActivating);
        setFocusPolicy(Qt::NoFocus);
        const HWND hwnd = reinterpret_cast<HWND>(winId());
        SetWindowLongPtrW(hwnd,GWL_EXSTYLE,GetWindowLongPtrW(hwnd,GWL_EXSTYLE) |
                         WS_EX_NOACTIVATE | WS_EX_TOOLWINDOW | WS_EX_TRANSPARENT);
    }
    resize(328,328);
}
void WheelWindow::present(quint64 session, Config config, Geometry geometry, const QString& name) {
    if (session <= session_) return;
    painted_ = false; session_ = session; config_ = std::move(config); selection_ = -1;
    for (auto* screen : QGuiApplication::screens()) {
        if (screen->name() != name) continue;
        setScreen(screen);
        resize(qRound(geometry.radius*2/screen->devicePixelRatio()),
               qRound(geometry.radius*2/screen->devicePixelRatio()));
        break;
    }
    const HWND hwnd = reinterpret_cast<HWND>(winId());
    SetWindowPos(hwnd,HWND_TOPMOST,qRound(geometry.center.x()-geometry.radius),
                 qRound(geometry.center.y()-geometry.radius),qRound(geometry.radius*2),
                 qRound(geometry.radius*2),SWP_NOACTIVATE);
    show();
    SetWindowPos(hwnd,HWND_TOPMOST,qRound(geometry.center.x()-geometry.radius),
                 qRound(geometry.center.y()-geometry.radius),qRound(geometry.radius*2),
                 qRound(geometry.radius*2),SWP_NOACTIVATE | SWP_SHOWWINDOW);
    update();
}
void WheelWindow::select(quint64 session, int index) {
    if (session != session_ || selection_ == index) return;
    selection_ = index; update();
}
void WheelWindow::dismiss(quint64 session) {
    if (session >= session_) { session_ = session; hide(); }
    Q_EMIT hidden(session);
}
void WheelWindow::preview(const Config& config) { config_ = config; update(); }
bool WheelWindow::nativeEvent(const QByteArray& type, void* message, qintptr* result) {
    const auto* msg = static_cast<MSG*>(message);
    if (overlay_ && msg->message == WM_MOUSEACTIVATE) { *result = MA_NOACTIVATE; return true; }
    if (overlay_ && msg->message == WM_NCHITTEST) { *result = HTTRANSPARENT; return true; }
    return QWidget::nativeEvent(type,message,result);
}
void WheelWindow::paintEvent(QPaintEvent*) {
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);
    const auto colors = themeColors(config_.theme);
    p.translate(width()/2.0,height()/2.0);
    p.scale(width()/328.0,height()/328.0);
    const QRectF outer(-164,-164,328,328);
    const QRectF inner(-42,-42,84,84);
    p.setPen(Qt::NoPen);
    for (int i=0;i<8;++i) {
        QPainterPath wedge;
        wedge.arcMoveTo(outer,112.5-i*45);
        wedge.arcTo(outer,112.5-i*45,-45);
        wedge.arcTo(inner,67.5-i*45,45);
        wedge.closeSubpath();
        const bool selected = i==selection_ && config_.slots[i].enabled();
        p.fillPath(wedge,selected ? colors.selected : colors.surface);
        const double a = i*std::numbers::pi/4;
        const QPointF center(108*std::sin(a),-108*std::cos(a));
        p.setPen(selected ? colors.selectedText : colors.text);
        QFont font = p.font(); font.setPixelSize(14); font.setWeight(QFont::DemiBold); p.setFont(font);
        const auto& slot = config_.slots[i];
        QString label = slot.enabled() ? slot.name : QStringLiteral("空");
        label = p.fontMetrics().elidedText(label,Qt::ElideRight,90);
        p.drawText(QRectF(center.x()-46,center.y()-20,92,22),Qt::AlignCenter,label);
        font.setPixelSize(10); font.setWeight(QFont::Normal); p.setFont(font);
        p.setPen(selected ? colors.selectedText : colors.muted);
        p.drawText(QRectF(center.x()-47,center.y()+4,94,18),Qt::AlignCenter,slot.kind==ActionKind::Screenshot ? QStringLiteral("截图") : shortcutText(slot.shortcut));
        p.setPen(Qt::NoPen);
    }
    p.setBrush(colors.background); p.drawEllipse(inner);
    p.setPen(colors.muted);
    QFont font = p.font(); font.setPixelSize(12); p.setFont(font);
    p.drawText(inner,Qt::AlignCenter,QStringLiteral("取消"));
    p.end();
    if (overlay_ && !painted_) { painted_ = true; Q_EMIT firstPaint(session_,monotonicNanos()); }
}
}
