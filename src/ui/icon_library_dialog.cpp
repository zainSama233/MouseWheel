#include <QCoreApplication>
#include "ui/icon_library_dialog.h"
#include "ui/library_icon.h"
#include <QListWidget>
#include <QLineEdit>
#include <QLabel>
#include <QPushButton>
#include <QVBoxLayout>
#include <QFileDialog>
#include <QInputDialog>
#include <QMessageBox>
namespace wheel {
IconLibraryDialog::IconLibraryDialog(ConfigStore& store,QWidget* parent):QDialog(parent),store_(store) {
    setAttribute(Qt::WA_DeleteOnClose);setWindowTitle(QCoreApplication::translate("MouseWheel","图标库"));resize(560,440);
    auto* root=new QVBoxLayout(this);search_=new QLineEdit;search_->setPlaceholderText(QCoreApplication::translate("MouseWheel","搜索图标"));root->addWidget(search_);
    list_=new QListWidget;list_->setObjectName("icon-library-list");list_->setViewMode(QListView::IconMode);list_->setResizeMode(QListView::Adjust);list_->setIconSize({48,48});list_->setGridSize({100,86});root->addWidget(list_,1);
    auto* bar=new QHBoxLayout;root->addLayout(bar);
    auto* import=new QPushButton(QCoreApplication::translate("MouseWheel","导入图标…"));import->setObjectName("icon-library-import");auto* rename=new QPushButton(QCoreApplication::translate("MouseWheel","重命名"));auto* remove=new QPushButton(QCoreApplication::translate("MouseWheel","删除"));auto* choose=new QPushButton(QCoreApplication::translate("MouseWheel","使用图标"));choose->setObjectName("icon-library-select");
    for(auto* button:{import,rename,remove,choose})bar->addWidget(button);
    status_=new QLabel;status_->setWordWrap(true);root->addWidget(status_);
    connect(search_,&QLineEdit::textChanged,this,&IconLibraryDialog::refresh);
    connect(import,&QPushButton::clicked,this,[this]{
        const auto files=QFileDialog::getOpenFileNames(this,QCoreApplication::translate("MouseWheel","导入图标"),{},QCoreApplication::translate("MouseWheel","图标 (*.svg *.png *.ico *.jpg *.jpeg)"));
        for(const auto& file:files)if(store_.importIcon(file).isEmpty()) {status_->setText(store_.error());break;}refresh();
    });
    connect(rename,&QPushButton::clicked,this,[this]{if(!list_->currentItem())return;
        bool ok=false;const auto name=QInputDialog::getText(this,QCoreApplication::translate("MouseWheel","重命名图标"),QCoreApplication::translate("MouseWheel","名称"),QLineEdit::Normal,list_->currentItem()->text(),&ok);
        if(ok && !store_.renameIcon(list_->currentItem()->data(Qt::UserRole).toString(),name))status_->setText(store_.error());refresh();
    });
    connect(remove,&QPushButton::clicked,this,[this]{if(!list_->currentItem())return;
        const auto id=list_->currentItem()->data(Qt::UserRole).toString();const int count=store_.iconReferences(id);
        const auto message=count?QCoreApplication::translate("MouseWheel","此图标被 %1 个槽位使用。删除后这些槽位恢复自动图标，继续？").arg(count):QCoreApplication::translate("MouseWheel","删除此图标？");
        if(QMessageBox::question(this,QCoreApplication::translate("MouseWheel","删除图标"),message)!=QMessageBox::Yes)return;
        if(!store_.removeIcon(id,true))status_->setText(store_.error());refresh();
    });
    const auto select=[this]{if(!list_->currentItem())return;Q_EMIT chosen(list_->currentItem()->data(Qt::UserRole).toString());accept();};
    connect(choose,&QPushButton::clicked,this,select);connect(list_,&QListWidget::itemDoubleClicked,this,select);refresh();
}
void IconLibraryDialog::refresh() {
    const QString selected=list_->currentItem()?list_->currentItem()->data(Qt::UserRole).toString():QString{};list_->clear();
    for(const auto& asset:store_.current().assets)if(asset.name.contains(search_->text(),Qt::CaseInsensitive)) {
        auto* item=new QListWidgetItem(libraryIcon(store_.assetPath(asset.id)),asset.name,list_);item->setData(Qt::UserRole,asset.id);if(asset.id==selected)list_->setCurrentItem(item);
    }
    if(!list_->currentItem() && list_->count())list_->setCurrentRow(0);
}
}
