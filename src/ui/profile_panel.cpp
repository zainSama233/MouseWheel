#include <QCoreApplication>
#include "ui/profile_panel.h"
#include "ui/application_picker.h"
#include <QComboBox>
#include <QListWidget>
#include <QPushButton>
#include <QVBoxLayout>
#include <QInputDialog>
#include <QMessageBox>
#include <QSignalBlocker>
#include <QEvent>
#include <QUuid>
namespace wheel {
ProfilePanel::ProfilePanel(ConfigStore& store,std::function<bool()> beforeChange,QWidget* parent)
    :QWidget(parent),store_(store),beforeChange_(std::move(beforeChange)) {
    auto* root=new QVBoxLayout(this);root->setContentsMargins(0,0,0,0);auto* bar=new QHBoxLayout;
    selector_=new QComboBox;selector_->setProperty("locale-user-items",true);selector_->setObjectName("profile-selector");bar->addWidget(selector_,1);
    auto* create=new QPushButton(QCoreApplication::translate("MouseWheel","新建"));auto* copy=new QPushButton(QCoreApplication::translate("MouseWheel","复制"));
    rename_=new QPushButton(QCoreApplication::translate("MouseWheel","重命名"));remove_=new QPushButton(QCoreApplication::translate("MouseWheel","删除"));
    for(auto* b:{create,copy,rename_,remove_})bar->addWidget(b);root->addLayout(bar);
    applications_=new QListWidget;applications_->setMaximumHeight(72);applications_->setObjectName("profile-applications");root->addWidget(applications_);
    auto* bindings=new QHBoxLayout;bind_=new QPushButton(QCoreApplication::translate("MouseWheel","绑定应用 / 运行窗口…"));unbind_=new QPushButton(QCoreApplication::translate("MouseWheel","移除绑定"));
    bindings->addWidget(bind_);bindings->addWidget(unbind_);bindings->addStretch();root->addLayout(bindings);
    connect(selector_,&QComboBox::currentIndexChanged,this,[this]{
        const auto next=selector_->currentData().toString();if(next==id_)return;
        if(!beforeChange_()){refresh();return;}id_=next;refresh();Q_EMIT selected();
    });
    for(auto* button:{create,copy})connect(button,&QPushButton::clicked,this,[this,button,copy]{
        if(!beforeChange_())return;bool ok=false;
        const auto name=QInputDialog::getText(this,QCoreApplication::translate("MouseWheel","配置方案"),QCoreApplication::translate("MouseWheel","名称"),QLineEdit::Normal,{},&ok).trimmed();
        if(!ok || name.isEmpty())return;auto c=store_.current();Profile profile;profile.id=QUuid::createUuid().toString(QUuid::WithoutBraces);profile.name=name;
        if(button==copy) {profile.wheel=c;for(const auto& p:c.profiles)if(p.id==id_)profile.wheel=p.wheel;}
        c.profiles.append(profile);if(!store_.commit(c)){QMessageBox::warning(this,QCoreApplication::translate("MouseWheel","保存失败"),store_.error());return;}
        id_=profile.id;refresh();Q_EMIT selected();
    });
    connect(rename_,&QPushButton::clicked,this,[this]{
        if(!beforeChange_())return;auto c=store_.current();for(auto& p:c.profiles)if(p.id==id_) {
            bool ok=false;const auto name=QInputDialog::getText(this,QCoreApplication::translate("MouseWheel","重命名方案"),QCoreApplication::translate("MouseWheel","名称"),QLineEdit::Normal,p.name,&ok).trimmed();
            if(!ok || name.isEmpty())return;p.name=name;break;
        }if(!store_.commit(c))QMessageBox::warning(this,QCoreApplication::translate("MouseWheel","保存失败"),store_.error());
    });
    connect(remove_,&QPushButton::clicked,this,[this]{
        if(!beforeChange_() || QMessageBox::question(this,QCoreApplication::translate("MouseWheel","删除方案"),QCoreApplication::translate("MouseWheel","删除当前方案及其应用绑定？"))!=QMessageBox::Yes)return;
        auto c=store_.current();c.profiles.removeIf([this](const auto& p){return p.id==id_;});
        if(!store_.commit(c)){QMessageBox::warning(this,QCoreApplication::translate("MouseWheel","保存失败"),store_.error());return;}id_.clear();refresh();Q_EMIT selected();
    });
    connect(bind_,&QPushButton::clicked,this,[this]{
        if(!beforeChange_())return;const auto target=id_;auto* picker=new ApplicationPicker(ApplicationPicker::Purpose::Binding,this);
        connect(picker,&ApplicationPicker::chosen,this,[this,target](const ApplicationEntry& entry,bool){
            auto c=store_.current();for(auto& p:c.profiles)if(p.id==target)p.applications.append(entry.executable);
            if(!store_.commit(c))QMessageBox::warning(this,QCoreApplication::translate("MouseWheel","绑定失败"),store_.error());
        });picker->open();
    });
    connect(unbind_,&QPushButton::clicked,this,[this]{
        if(!applications_->currentItem() || !beforeChange_())return;auto c=store_.current();
        for(auto& p:c.profiles)if(p.id==id_)p.applications.removeAll(applications_->currentItem()->text());
        if(!store_.commit(c))QMessageBox::warning(this,QCoreApplication::translate("MouseWheel","保存失败"),store_.error());
    });
    connect(&store_,&ConfigStore::changed,this,&ProfilePanel::refresh);refresh();
}
void ProfilePanel::changeEvent(QEvent* event){if(event->type()==QEvent::LanguageChange)refresh();QWidget::changeEvent(event);}
void ProfilePanel::refresh() {
    const QSignalBlocker blocker(selector_);selector_->clear();selector_->addItem(QCoreApplication::translate("MouseWheel","全局方案"),QString{});
    for(const auto& p:store_.current().profiles)selector_->addItem(p.name,p.id);
    selector_->setCurrentIndex(qMax(0,selector_->findData(id_)));applications_->clear();
    for(const auto& p:store_.current().profiles)if(p.id==id_)applications_->addItems(p.applications);
    for(auto* b:{rename_,remove_,bind_,unbind_})b->setEnabled(!id_.isEmpty());
    applications_->setVisible(!id_.isEmpty());bind_->setVisible(!id_.isEmpty());unbind_->setVisible(!id_.isEmpty());
}
}
