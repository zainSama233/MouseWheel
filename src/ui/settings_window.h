#pragma once
#include <QWidget>
#include "config/config_store.h"
class QComboBox;
class QListWidget;
class QLabel;
class QLineEdit;
class QKeySequenceEdit;
class QPushButton;
namespace wheel {
class WheelWindow;
class SlotEditor;
class SettingsWindow final : public QWidget {
    Q_OBJECT
public:
    explicit SettingsWindow(ConfigStore& store);
private:
    void submit();
    void populate();
    ConfigStore& store_;
    QListWidget* slots_;
    QComboBox* modifier_;
    QComboBox* button_;
    QComboBox* theme_;
    QComboBox* shape_;
    QByteArray centerImage_;
    std::array<SlotEditor*,8> editors_{};
    QLabel* status_;
    WheelWindow* preview_;
    bool populating_ = false;
};
}
