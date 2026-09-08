#include "ui/trigger_rules_editor.h"
#include "ui/application_picker.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QCheckBox>
#include <QListWidget>
#include <QPushButton>
#include <QFileInfo>
#include <QDir>
namespace wheel {
TriggerRulesEditor::TriggerRulesEditor(QWidget* parent):QWidget(parent) {
    auto* root=new QVBoxLayout(this); root->setContentsMargins(0,0,0,0);
    auto* row=new QHBoxLayout; fullscreen_=new QCheckBox(QStringLiteral("全屏时暂停轮盘")); fullscreen_->setObjectName("pause-fullscreen");
    auto* add=new QPushButton(QStringLiteral("添加暂停应用…")); add->setObjectName("exclude-application");
    auto* remove=new QPushButton(QStringLiteral("移除")); remove->setObjectName("remove-excluded-application"); remove->setEnabled(false);
    row->addWidget(fullscreen_); row->addStretch(); row->addWidget(add); row->addWidget(remove); root->addLayout(row);
    excluded_=new QListWidget; excluded_->setObjectName("excluded-applications"); excluded_->setMaximumHeight(90); excluded_->setFrameShape(QFrame::NoFrame); excluded_->hide(); root->addWidget(excluded_);
    connect(fullscreen_,&QCheckBox::toggled,this,[this]{if(!loading_) Q_EMIT edited();});
    connect(excluded_,&QListWidget::currentRowChanged,this,[remove](int row){remove->setEnabled(row>=0);});
    connect(remove,&QPushButton::clicked,this,[this]{delete excluded_->takeItem(excluded_->currentRow()); excluded_->setFixedHeight(qMin(3,excluded_->count())*30+8); excluded_->setVisible(excluded_->count()>0); Q_EMIT edited();});
    connect(add,&QPushButton::clicked,this,[this]{
        auto* picker=new ApplicationPicker(ApplicationPicker::Purpose::Exclusion,this);
        connect(picker,&ApplicationPicker::chosen,this,[this](const ApplicationEntry& entry,bool){
            auto current=rules(); const auto path=QDir::cleanPath(QDir::fromNativeSeparators(entry.executable));
            if(path.isEmpty() || current.excludedApplications.contains(path,Qt::CaseInsensitive)) return;
            current.excludedApplications.append(path); setRules(current); Q_EMIT edited();
        }); picker->open();
    });
}
void TriggerRulesEditor::setRules(const TriggerRules& rules) {
    loading_=true; fullscreen_->setChecked(rules.pauseFullscreen); excluded_->clear();
    for(const auto& path:rules.excludedApplications) {
        auto* item=new QListWidgetItem(QFileInfo(path).completeBaseName()+"  ·  "+path,excluded_); item->setData(Qt::UserRole,path); item->setToolTip(path);
    }
    excluded_->setFixedHeight(qMin(3,excluded_->count())*30+8); excluded_->setVisible(excluded_->count()>0); loading_=false;
}
TriggerRules TriggerRulesEditor::rules() const {
    TriggerRules value; value.pauseFullscreen=fullscreen_->isChecked();
    for(int i=0;i<excluded_->count();++i) value.excludedApplications.append(excluded_->item(i)->data(Qt::UserRole).toString());
    return value;
}
}
