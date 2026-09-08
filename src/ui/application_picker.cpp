#include <QCoreApplication>
#include "ui/application_picker.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLineEdit>
#include <QListWidget>
#include <QComboBox>
#include <QLabel>
#include <QPushButton>
#include <QCheckBox>
#include <QFutureWatcher>
#include <QtConcurrentRun>
#include <QFileDialog>
#include <QFileInfo>
namespace wheel {
ApplicationPicker::ApplicationPicker(Purpose purpose,QWidget* parent):QDialog(parent) {
    setObjectName("application-picker"); setWindowTitle(QCoreApplication::translate("MouseWheel","选择应用")); resize(650,500);
    setAttribute(Qt::WA_DeleteOnClose);
    auto* root=new QVBoxLayout(this); auto* toolbar=new QHBoxLayout;
    source_=new QComboBox; source_->setObjectName("application-source"); source_->addItems({QCoreApplication::translate("MouseWheel","已安装应用"),QCoreApplication::translate("MouseWheel","运行中的窗口")});
    search_=new QLineEdit; search_->setObjectName("application-search"); search_->setPlaceholderText(QCoreApplication::translate("MouseWheel","搜索名称或路径"));
    refresh_=new QPushButton(QCoreApplication::translate("MouseWheel","刷新")); toolbar->addWidget(source_); toolbar->addWidget(search_,1); toolbar->addWidget(refresh_); root->addLayout(toolbar);
    list_=new QListWidget; list_->setObjectName("application-results"); list_->setAlternatingRowColors(true); list_->setUniformItemSizes(true); list_->setTextElideMode(Qt::ElideMiddle); list_->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff); root->addWidget(list_,1);
    status_=new QLabel; status_->setObjectName("application-status"); root->addWidget(status_);
    useIcon_=new QCheckBox(QCoreApplication::translate("MouseWheel","使用程序图标")); useIcon_->setObjectName("application-use-icon"); useIcon_->setVisible(purpose==Purpose::Launch); root->addWidget(useIcon_);
    auto* footer=new QHBoxLayout; auto* browse=new QPushButton(QCoreApplication::translate("MouseWheel","浏览 EXE…")); auto* cancel=new QPushButton(QCoreApplication::translate("MouseWheel","取消"));
    select_=new QPushButton(QCoreApplication::translate("MouseWheel","选择")); select_->setObjectName("application-select"); select_->setEnabled(false); select_->setDefault(true);
    footer->addWidget(browse); footer->addStretch(); footer->addWidget(cancel); footer->addWidget(select_); root->addLayout(footer);
    connect(browse,&QPushButton::clicked,this,[this]{
#ifdef Q_OS_MACOS
        const auto path=QFileDialog::getOpenFileName(this,QCoreApplication::translate("MouseWheel","选择程序"),"/Applications","Applications (*.app);;All files (*)");
#else
        const auto path=QFileDialog::getOpenFileName(this,QCoreApplication::translate("MouseWheel","选择程序"),{},QCoreApplication::translate("MouseWheel","程序 (*.exe)"));
#endif
        if(path.isEmpty()) return;
        Q_EMIT chosen({QFileInfo(path).completeBaseName(),path,path},useIcon_->isChecked()); accept();
    });
    connect(cancel,&QPushButton::clicked,this,&QDialog::reject);
    connect(select_,&QPushButton::clicked,this,&ApplicationPicker::choose);
    connect(list_,&QListWidget::itemDoubleClicked,this,[this]{choose();});
    connect(list_,&QListWidget::currentRowChanged,this,[this](int row){select_->setEnabled(row>=0);});
    connect(search_,&QLineEdit::textChanged,this,&ApplicationPicker::filter);
    connect(source_,&QComboBox::currentIndexChanged,this,&ApplicationPicker::reload);
    connect(refresh_,&QPushButton::clicked,this,&ApplicationPicker::reload); reload();
}
ApplicationPicker::~ApplicationPicker() { if(cancelled_) cancelled_->store(true); }
void ApplicationPicker::reload() {
    if(cancelled_) cancelled_->store(true);
    cancelled_=std::make_shared<std::atomic_bool>(false); const auto cancelled=cancelled_;
    entries_.clear(); list_->clear(); select_->setEnabled(false); refresh_->setEnabled(false); status_->setText(QCoreApplication::translate("MouseWheel","正在读取应用…"));
    auto* watcher=new QFutureWatcher<QList<ApplicationEntry>>(this);
    connect(watcher,&QFutureWatcher<QList<ApplicationEntry>>::finished,this,[this,watcher,cancelled]{
        watcher->deleteLater(); if(cancelled->load()) return;
        entries_=watcher->result(); refresh_->setEnabled(true); filter();
    });
    const bool running=source_->currentIndex()==1;
    watcher->setFuture(QtConcurrent::run([running,cancelled]{return running?platform::runningApplications():platform::discoverApplications(platform::applicationShortcutRoots(),cancelled.get());}));
}
void ApplicationPicker::filter() {
    list_->clear(); const auto query=search_->text().trimmed();
    for(int i=0;i<entries_.size();++i) {
        const auto& entry=entries_[i];
        if(!entry.name.contains(query,Qt::CaseInsensitive) && !entry.path.contains(query,Qt::CaseInsensitive)) continue;
        auto* item=new QListWidgetItem(entry.name+"\n"+entry.path,list_); item->setData(Qt::UserRole,i); item->setToolTip(entry.path);
    }
    if(list_->count()) list_->setCurrentRow(0);
    select_->setEnabled(list_->currentRow()>=0);
    if(refresh_->isEnabled()) status_->setText(list_->count()?QCoreApplication::translate("MouseWheel","%1 个结果").arg(list_->count()):QCoreApplication::translate("MouseWheel","未找到应用，可刷新或浏览 EXE"));
}
void ApplicationPicker::choose() {
    const auto* item=list_->currentItem(); if(!item) return;
    const auto entry=entries_.at(item->data(Qt::UserRole).toInt());
    Q_EMIT chosen(entry,useIcon_->isChecked()); accept();
}
}
