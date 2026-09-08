#pragma once
#include <QWidget>
#include "core/model.h"
class QLabel; class QTimer; class QPushButton;
class QLineEdit; class QComboBox; class QCheckBox; class QPlainTextEdit; class QSpinBox; class QStackedWidget;
namespace wheel {
class ConfigStore; class StyleEditor;
class ShortcutEditor;
class WebsiteIcon;
class SlotEditor final:public QWidget {
    Q_OBJECT
public:
    SlotEditor(int index,QWidget* parent=nullptr);
    void setSlot(const Slot& slot);
    Slot slot() const;
    void setGroupsAllowed(bool allowed);
    void setStore(ConfigStore* store);
Q_SIGNALS:
    void edited();
    void editGroup();
private:
    void refreshAutomaticIcon();
    ConfigStore* store_=nullptr;
    QPushButton* libraryButton_;
    QString libraryId_;
    StyleEditor* styleEditor_;
    WebsiteIcon* websiteIcon_=nullptr;
    QTimer* iconTimer_;
    QLabel* iconStatus_;
    QPushButton* fetchIcon_;
    QString automaticUrl_;
    QByteArray automaticImage_;
    QLineEdit *name_,*appPath_,*arguments_,*directory_,*url_,*browserPath_,*folderPath_,*commandDirectory_,*endpoint_,*apiKey_,*model_,*resultPath_,*iconProgram_;
    QComboBox *kind_,*browser_,*folder_,*shell_,*provider_,*window_,*system_,*iconSource_,*symbol_;
    QCheckBox *normal_,*hidden_,*label_;
    QPlainTextEdit* script_;
    QSpinBox* opacity_;
    QStackedWidget *pages_,*icons_;
    ShortcutEditor* shortcut_;
    GroupAction group_;
    QByteArray image_;
    bool loading_=false;
};
}
