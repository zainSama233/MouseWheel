#pragma once
#include <QWidget>
#include "config/config_store.h"
class QComboBox;
class QLabel;
class QLineEdit;
class QKeySequenceEdit;
class QPushButton;
namespace wheel {
class WheelWindow;
class SettingsWindow final : public QWidget {
    Q_OBJECT
public:
    explicit SettingsWindow(ConfigStore& store);
private:
    void submit();
    void populate();
    ConfigStore& store_;
    QComboBox* modifier_;
    QComboBox* button_;
    QComboBox* theme_;
    QComboBox* shape_;
    QByteArray centerImage_;
    std::array<QComboBox*,8> kinds_{};
    std::array<QLineEdit*,8> names_{};
    std::array<QKeySequenceEdit*,8> shortcuts_{};
    std::array<QLineEdit*,8> targets_{};
    std::array<QPushButton*,8> browse_{};
    QLabel* status_;
    WheelWindow* preview_;
    bool populating_ = false;
};
}
