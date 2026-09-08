#include "app.h"
#include "ui/settings_window.h"
#include "ui/theme.h"
#include "tools/launcher.h"
#include <QApplication>
#include <QAction>
#include <QMenu>
#include <QPainter>
#include <QFile>
namespace wheel {
App::App(QString configPath) : config_(std::move(configPath)), menu_(std::make_unique<QMenu>()) {
    QPixmap icon(48,48); icon.fill(Qt::transparent);
    QPainter p(&icon); p.setRenderHint(QPainter::Antialiasing); p.setPen(Qt::NoPen);
    p.setBrush(QColor("#476dec")); p.drawEllipse(3,3,42,42);
    p.setBrush(Qt::white); p.drawEllipse(19,19,10,10);
    for (int i=0;i<8;++i) { p.save(); p.translate(24,24); p.rotate(i*45); p.drawRoundedRect(-2,-18,4,8,2,2); p.restore(); }
    p.end(); tray_.setIcon(QIcon(icon)); QApplication::setWindowIcon(QIcon(icon));
    menu_->addAction(QStringLiteral("打开设置"),this,&App::openSettings);
    menu_->addAction(actionKindName(ActionKind::Screenshot),this,&App::takeScreenshot);
    menu_->addAction(actionKindName(ActionKind::ScreenAnnotation),this,&App::annotateScreen);
    connect(&input_,&InputService::actionRequested,this,[this](const Slot& action){
        if(action.kind==ActionKind::Screenshot) takeScreenshot();
        else if(action.kind==ActionKind::ScreenAnnotation) annotateScreen();
        else {
            QString error; if(!launchTarget(action,error)) tray_.showMessage(QStringLiteral("打开失败"),error,QSystemTrayIcon::Warning);
        }
    });
    connect(&annotation_,&ScreenAnnotationSession::stateChanged,this,&App::updateState);
    connect(&screenshot_,&ScreenshotSession::activeChanged,this,&App::updateState);
    pauseAction_ = menu_->addAction(QStringLiteral("暂停")); pauseAction_->setCheckable(true);
    connect(pauseAction_,&QAction::toggled,this,[this](bool checked){ paused_ = checked; updateState(); });
    menu_->addAction(QStringLiteral("重新连接输入"),&input_,&InputService::restart);
    menu_->addSeparator(); menu_->addAction(QStringLiteral("退出"),qApp,&QCoreApplication::quit);
    tray_.setContextMenu(menu_.get());
    connect(&tray_,&QSystemTrayIcon::activated,this,[this](auto reason){
        if (reason == QSystemTrayIcon::DoubleClick || reason == QSystemTrayIcon::Trigger) openSettings();
    });
    connect(&input_,&InputService::showWheel,&wheel_,&WheelWindow::present);
    connect(&input_,&InputService::selection,&wheel_,&WheelWindow::select);
    connect(&input_,&InputService::hideWheel,&wheel_,&WheelWindow::dismiss);
    connect(&wheel_,&WheelWindow::hidden,&input_,&InputService::hidden);
    connect(&input_,&InputService::failure,this,[this](const QString& error){
        qWarning().noquote() << error;
        tray_.showMessage(QStringLiteral("鼠标快捷强化"),error,QSystemTrayIcon::Warning);
    });
    connect(&input_,&InputService::listening,this,[this](bool available){
        available_ = available; updateState();
    });
    connect(&config_,&ConfigStore::changed,&input_,&InputService::configure);
    connect(&config_,&ConfigStore::changed,this,[this]{ wheel_.preview(config_.current()); updateState(); });
}
App::~App() {
    if (settings_) { disconnect(settings_,nullptr,this,nullptr); delete settings_.data(); }
}
void App::start(bool showSettings) {
    const bool exists = QFile::exists(config_.path());
    const bool loaded = config_.load();
    if (loaded && !exists && !config_.commit(config_.current()))
        qWarning().noquote() << config_.error();
    tray_.show();
    wheel_.preview(config_.current());
    input_.start(config_.current());
    updateState();
    if (showSettings || !exists || !loaded) openSettings();
}
void App::takeScreenshot() {
    if(screenshot_.active()) return;
    annotation_.setDrawing(false);
    QString error;
    if(!screenshot_.start(config_.current().theme,error) && !error.isEmpty())
        tray_.showMessage(QStringLiteral("截图失败"),error,QSystemTrayIcon::Warning);
}
void App::annotateScreen() {
    screenshot_.cancel(); annotation_.start(config_.current().theme);
}
void App::openSettings() {
    screenshot_.cancel(); annotation_.setDrawing(false);
    if (!settings_) {
        settings_ = new SettingsWindow(config_);
        connect(settings_,&QObject::destroyed,this,[this]{ settings_ = nullptr; updateState(); });
    }
    updateState(); settings_->showNormal(); settings_->raise(); settings_->activateWindow();
}
void App::updateState() {
    const bool paused = paused_ || settings_ || screenshot_.active() || annotation_.drawing() || config_.blocked();
    input_.pause(paused);
    const QString state = !available_ ? QStringLiteral("输入不可用") :
                          paused ? QStringLiteral("已暂停") : QStringLiteral("运行中");
    tray_.setToolTip(QStringLiteral("鼠标快捷强化 · ") + state);
    pauseAction_->setText(paused_ ? QStringLiteral("恢复") : QStringLiteral("暂停"));
}
}
