#pragma once
#include <QWidget>
#include "config/config_store.h"
class QComboBox;
class QLabel;
class QLineEdit;
class QKeySequenceEdit;
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
    std::array<QComboBox*,8> kinds_{};
    std::array<QLineEdit*,8> names_{};
    std::array<QKeySequenceEdit*,8> shortcuts_{};
    QLabel* status_;
    WheelWindow* preview_;
    bool populating_ = false;
};
}
