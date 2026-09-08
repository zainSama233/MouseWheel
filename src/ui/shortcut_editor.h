#pragma once
#include <QWidget>
#include <QPointer>
#include "core/model.h"
class QKeySequenceEdit;
class QComboBox;
class QCheckBox;
class QPushButton;
namespace wheel {
class ShortcutCapture;
class ShortcutEditor final:public QWidget {
    Q_OBJECT
public:
    explicit ShortcutEditor(QWidget* parent=nullptr);
    void setShortcut(Shortcut value);
    Shortcut shortcut() const;
Q_SIGNALS:
    void edited();
protected:
    void hideEvent(QHideEvent* event) override;
private:
    QPointer<ShortcutCapture> capture_;
    QKeySequenceEdit* sequence_;
    QComboBox* key_;
    std::array<QCheckBox*,4> modifiers_;
    QPushButton* record_;
    bool loading_=false;
};
}
