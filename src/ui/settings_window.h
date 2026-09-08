#pragma once
#include <QWidget>
#include "config/config_store.h"
class QComboBox; class QCheckBox; class QDoubleSpinBox;
class QStackedWidget;
class QListWidget;
class QLabel;
class QLineEdit;
class QKeySequenceEdit;
class QPushButton;
namespace wheel {
class WheelWindow;
class SlotEditor;
class TriggerRulesEditor; class ProfilePanel; class StyleEditor;
class SettingsWindow final : public QWidget {
    Q_OBJECT
public:
    explicit SettingsWindow(ConfigStore& store);
private:
    void submit();
    void populate();
    bool pageComplete();
    WheelConfig& wheel(Config& config) const;
    QList<Slot>& page(Config& config) const;
    ConfigStore& store_;
    QListWidget* slots_;
    QComboBox* modifier_;
    QComboBox* button_;
    QComboBox* theme_;
    QComboBox* language_;
    QComboBox* shape_;
    QComboBox* count_;
    QComboBox* navigation_;
    QStackedWidget* pages_;
    ProfilePanel* profiles_;
    StyleEditor* styleEditor_;
    QCheckBox *centerEnabled_,*frosted_;
    QDoubleSpinBox *deadZone_,*marginX_,*marginY_;
    QComboBox* edgePolicy_;
    int group_=-1;
    QByteArray centerImage_;
    std::array<SlotEditor*,13> editors_{};
    QLabel* status_;
    WheelWindow* preview_;
    TriggerRulesEditor* rules_;
    bool populating_ = false;
};
}
