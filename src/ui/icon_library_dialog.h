#pragma once
#include <QDialog>
#include "config/config_store.h"
class QListWidget;class QLineEdit;class QLabel;
namespace wheel {
class IconLibraryDialog final:public QDialog {
    Q_OBJECT
public:
    explicit IconLibraryDialog(ConfigStore& store,QWidget* parent=nullptr);
Q_SIGNALS:
    void chosen(QString id);
private:
    void refresh();
    ConfigStore& store_;
    QListWidget* list_;
    QLineEdit* search_;
    QLabel* status_;
};
}
