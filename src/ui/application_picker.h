#pragma once
#include <QDialog>
#include <memory>
#include "platform/application_catalog.h"
class QLineEdit; class QListWidget; class QComboBox; class QLabel; class QPushButton; class QCheckBox;
namespace wheel {
class ApplicationPicker final:public QDialog {
    Q_OBJECT
public:
    enum class Purpose { Launch, Exclusion };
    explicit ApplicationPicker(Purpose purpose,QWidget* parent=nullptr);
    ~ApplicationPicker() override;
Q_SIGNALS:
    void chosen(const wheel::ApplicationEntry& entry,bool useProgramIcon);
private:
    void reload();
    void filter();
    void choose();
    QList<ApplicationEntry> entries_;
    std::shared_ptr<std::atomic_bool> cancelled_;
    QLineEdit* search_;
    QListWidget* list_;
    QComboBox* source_;
    QLabel* status_;
    QPushButton *select_,*refresh_;
    QCheckBox* useIcon_;
};
}
