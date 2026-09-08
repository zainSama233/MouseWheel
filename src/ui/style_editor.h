#pragma once
#include <QWidget>
#include <array>
#include "core/model.h"
class QLineEdit;class QDoubleSpinBox;class QComboBox;
namespace wheel {
class ConfigStore;
class StyleEditor final:public QWidget {
    Q_OBJECT
public:
    explicit StyleEditor(QWidget* parent=nullptr);
    void setStore(ConfigStore* store) {store_=store;}
    void setStyle(const SlotStyle& style);
    SlotStyle style() const;
Q_SIGNALS:
    void edited();
private:
    ConfigStore* store_=nullptr;
    std::array<QLineEdit*,4> colors_{};
    std::array<std::optional<QColor>,4> colorValues_{};
    std::array<QDoubleSpinBox*,6> numbers_{};
    QLineEdit* font_;
    QComboBox* layout_;
    bool loading_=false;
};
}
