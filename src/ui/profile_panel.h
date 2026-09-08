#pragma once
#include <QWidget>
#include <functional>
#include "config/config_store.h"
class QComboBox;class QListWidget;class QPushButton;
namespace wheel {
class ProfilePanel final:public QWidget {
    Q_OBJECT
public:
    ProfilePanel(ConfigStore& store,std::function<bool()> beforeChange,QWidget* parent=nullptr);
    QString currentId() const {return id_;}
Q_SIGNALS:
    void selected();
protected:
    void changeEvent(QEvent*) override;
private:
    void refresh();
    ConfigStore& store_;
    std::function<bool()> beforeChange_;
    QString id_;
    QComboBox* selector_;
    QListWidget* applications_;
    QPushButton *rename_,*remove_,*bind_,*unbind_;
};
}
