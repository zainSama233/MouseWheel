#pragma once
#include <QWidget>
#include "core/model.h"
class QLineEdit; class QComboBox; class QCheckBox; class QPlainTextEdit; class QSpinBox; class QStackedWidget;
namespace wheel {
class ShortcutEditor;
class SlotEditor final:public QWidget {
    Q_OBJECT
public:
    SlotEditor(int index,QWidget* parent=nullptr);
    void setSlot(const Slot& slot);
    Slot slot() const;
Q_SIGNALS:
    void edited();
private:
    QLineEdit *name_,*appPath_,*arguments_,*directory_,*url_,*browserPath_,*folderPath_,*commandDirectory_,*endpoint_,*apiKey_,*model_,*resultPath_,*iconProgram_;
    QComboBox *kind_,*browser_,*folder_,*shell_,*provider_,*window_,*system_,*iconSource_,*symbol_;
    QCheckBox *normal_,*hidden_,*label_;
    QPlainTextEdit* script_;
    QSpinBox* opacity_;
    QStackedWidget *pages_,*icons_;
    ShortcutEditor* shortcut_;
    QByteArray image_;
    bool loading_=false;
};
}
