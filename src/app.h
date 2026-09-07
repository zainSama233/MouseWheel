#pragma once
#include <QObject>
#include <QPointer>
#include <QSystemTrayIcon>
#include "config/config_store.h"
#include "platform/input_service.h"
#include "ui/wheel_window.h"
#include "tools/screenshot_session.h"
class QAction;
class QMenu;
namespace wheel {
class SettingsWindow;
class App final : public QObject {
    Q_OBJECT
public:
    explicit App(QString configPath);
    ~App() override;
    void start(bool showSettings);
private:
    void openSettings();
    void takeScreenshot();
    void updateState();
    ConfigStore config_;
    InputService input_;
    WheelWindow wheel_;
    ScreenshotSession screenshot_;
    QSystemTrayIcon tray_;
    std::unique_ptr<QMenu> menu_;
    QPointer<SettingsWindow> settings_;
    QAction* pauseAction_;
    bool paused_ = false;
    bool available_ = false;
};
}
