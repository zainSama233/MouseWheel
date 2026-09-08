#pragma once
#include <QWidget>
#include "core/model.h"
class QCheckBox; class QListWidget;
namespace wheel {
class TriggerRulesEditor final:public QWidget {
    Q_OBJECT
public:
    explicit TriggerRulesEditor(QWidget* parent=nullptr);
    void setRules(const TriggerRules& rules);
    TriggerRules rules() const;
Q_SIGNALS:
    void edited();
private:
    QCheckBox* fullscreen_;
    QListWidget* excluded_;
    bool loading_=false;
};
}
