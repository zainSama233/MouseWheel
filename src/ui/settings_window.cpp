#include "ui/settings_window.h"
#include "ui/wheel_window.h"
#include "ui/slot_editor.h"
#include <QStackedWidget>
#include <QListWidget>
#include "ui/theme.h"
#include "core/image_asset.h"
#include <QFileDialog>
#include <QFileInfo>
#include <QUrl>
#include <QComboBox>
#include <QLabel>
#include <QLineEdit>
#include <QKeySequenceEdit>
#include <QPushButton>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QGridLayout>
#include <QMessageBox>
#include <QKeySequence>
#include <QScrollArea>
#include <QScreen>
#include <QSignalBlocker>
namespace wheel {
SettingsWindow::SettingsWindow(ConfigStore& store) : store_(store) {
    setAttribute(Qt::WA_DeleteOnClose);
    setWindowTitle(QStringLiteral("鼠标快捷强化 · 设置"));
    setMinimumSize(640,480);
    resize(1060,qMin(860,screen()->availableGeometry().height()-60));
    auto* root = new QVBoxLayout(this); root->setContentsMargins(0,0,0,0);
    auto* scroll = new QScrollArea; scroll->setWidgetResizable(true); scroll->setFrameShape(QFrame::NoFrame);
    auto* content = new QWidget; content->setMinimumWidth(900); scroll->setWidget(content); root->addWidget(scroll);
    auto* layout = new QVBoxLayout(content); layout->setContentsMargins(30,24,30,24); layout->setSpacing(18);
    auto* title = new QLabel(QStringLiteral("鼠标快捷强化")); title->setObjectName("title"); layout->addWidget(title);
    auto* toolbar = new QHBoxLayout;
    modifier_ = new QComboBox; button_ = new QComboBox; theme_ = new QComboBox;
    modifier_->addItem(QStringLiteral("无修饰键"),static_cast<int>(Modifier::None));
    modifier_->setObjectName("trigger-modifier"); button_->setObjectName("trigger-button");
    for (auto [name,value] : {std::pair{"Ctrl",Modifier::Control},{"Alt",Modifier::Alt},
                             {"Shift",Modifier::Shift},{"Win",Modifier::Meta}})
        modifier_->addItem(QString::fromLatin1(name),static_cast<int>(value));
    button_->addItems({QStringLiteral("鼠标右键"),QStringLiteral("鼠标中键"),QStringLiteral("侧键 · 后退"),QStringLiteral("侧键 · 前进")});
    theme_->addItems({QStringLiteral("明亮"),QStringLiteral("护眼"),QStringLiteral("深色")});
    toolbar->addWidget(new QLabel(QStringLiteral("按住"))); toolbar->addWidget(modifier_);
    toolbar->addWidget(new QLabel("+")); toolbar->addWidget(button_);
    toolbar->addStretch(); toolbar->addWidget(theme_); layout->addLayout(toolbar);
    auto* triggerHint = new QLabel(QStringLiteral("单键模式占用中键点击；滚轮照常使用。暂停后恢复中键原功能。"));
    triggerHint->setObjectName("muted"); layout->addWidget(triggerHint);
    auto* appearance=new QHBoxLayout;
    shape_=new QComboBox; shape_->setObjectName("wheel-shape");
    shape_->addItems({QStringLiteral("扇形槽位"),QStringLiteral("圆形槽位"),QStringLiteral("六边形槽位")});
    appearance->addWidget(shape_);
    auto* image=new QPushButton(QStringLiteral("中心图片…")); image->setObjectName("center-image");
    auto* clearImage=new QPushButton(QStringLiteral("恢复取消图标"));
    appearance->addWidget(image); appearance->addWidget(clearImage); appearance->addStretch(); layout->addLayout(appearance);
    connect(shape_,&QComboBox::currentIndexChanged,this,&SettingsWindow::submit);
    connect(image,&QPushButton::clicked,this,[this]{
        const auto path=QFileDialog::getOpenFileName(this,QStringLiteral("中心图片"),{},QStringLiteral("图片 (*.png *.jpg *.jpeg *.bmp)"));
        if(path.isEmpty()) return;
        QByteArray data; QString error;
        if(!importImageAsset(path,data,error)) { QMessageBox::warning(this,QStringLiteral("图片不可用"),error); return; }
        centerImage_=std::move(data); submit();
    });
    connect(clearImage,&QPushButton::clicked,this,[this]{centerImage_.clear(); submit();});
    auto* body = new QHBoxLayout;
    auto* slots=new QListWidget; slots_=slots; slots->setFrameShape(QFrame::NoFrame); slots->setSpacing(5); slots->setObjectName("slot-list"); slots->setFixedWidth(125);
    auto* pages=new QStackedWidget;
    for(int i=0;i<8;++i) {
        slots->addItem(QStringLiteral("槽位 %1").arg(i+1));
        editors_[i]=new SlotEditor(i); pages->addWidget(editors_[i]);
        connect(editors_[i],&SlotEditor::edited,this,&SettingsWindow::submit);
    }
    connect(slots,&QListWidget::currentRowChanged,pages,&QStackedWidget::setCurrentIndex); slots->setCurrentRow(0);
    body->addWidget(slots); body->addWidget(pages,1);
    auto* side = new QVBoxLayout;
    preview_ = new WheelWindow(false); preview_->setFixedSize(264,264);
    side->addStretch(); side->addWidget(preview_,0,Qt::AlignCenter);
    auto* hint = new QLabel(QStringLiteral("从上方起，顺时针排列")); hint->setObjectName("muted");
    side->addWidget(hint,0,Qt::AlignCenter); side->addStretch();
    body->addLayout(side); layout->addLayout(body,1);
    status_ = new QLabel; status_->setWordWrap(true); status_->setObjectName("muted"); layout->addWidget(status_);
    auto* footer = new QHBoxLayout;
    auto* location = new QLabel(QStringLiteral("配置位置")); location->setToolTip(store_.path()); location->setObjectName("muted");
    footer->addWidget(location); footer->addStretch();
    auto* reset = new QPushButton(QStringLiteral("恢复默认"));
    auto* close = new QPushButton(QStringLiteral("完成")); close->setObjectName("primary");
    footer->addWidget(reset); footer->addWidget(close); layout->addLayout(footer);
    connect(close,&QPushButton::clicked,this,[this]{
        submit();
        if (status_->property("failed").toBool()) return;
        QWidget::close();
    });
    connect(reset,&QPushButton::clicked,this,[this]{
        if (QMessageBox::question(this,QStringLiteral("恢复默认"),
            QStringLiteral("用默认配置替换当前配置文件？")) != QMessageBox::Yes) return;
        if (store_.reset()) populate(); else status_->setText(store_.error());
    });
    connect(modifier_,&QComboBox::currentIndexChanged,this,&SettingsWindow::submit);
    connect(button_,&QComboBox::currentIndexChanged,this,&SettingsWindow::submit);
    connect(theme_,&QComboBox::currentIndexChanged,this,&SettingsWindow::submit);
    populate();
}
void SettingsWindow::populate() {
    populating_ = true;
    const auto& config = store_.current();
    modifier_->setCurrentIndex(modifier_->findData(static_cast<int>(config.modifier)));
    button_->setCurrentIndex(static_cast<int>(config.button));
    button_->setEnabled(config.modifier != Modifier::None);
    theme_->setCurrentIndex(static_cast<int>(config.theme));
    shape_->setCurrentIndex(static_cast<int>(config.shape)); centerImage_=config.centerImage;
    for(int i=0;i<8;++i) { editors_[i]->setSlot(config.slots[i]); slots_->item(i)->setText(QString::number(i+1)+"  "+(config.slots[i].name.isEmpty()?QStringLiteral("空槽位"):config.slots[i].name)); }
    setStyleSheet(settingsStyle(config.theme)); preview_->preview(config);
    status_->setText(store_.blocked() ? store_.error() : QStringLiteral("自动保存 · 设置打开时暂停轮盘"));
    status_->setProperty("failed",store_.blocked());
    populating_ = false;
}
void SettingsWindow::submit() {
    if (populating_) return;
    Config draft;
    draft.modifier = static_cast<Modifier>(modifier_->currentData().toInt());
    if (draft.modifier == Modifier::None) {
        const QSignalBlocker blocker(button_);
        button_->setCurrentIndex(static_cast<int>(MouseButton::Middle));
    }
    button_->setEnabled(draft.modifier != Modifier::None);
    draft.button = static_cast<MouseButton>(button_->currentIndex());
    draft.theme = static_cast<Theme>(theme_->currentIndex());
    draft.shape=static_cast<WheelShape>(shape_->currentIndex()); draft.centerImage=centerImage_;
    for(int i=0;i<8;++i) draft.slots[i]=editors_[i]->slot();
    const bool ok = !store_.blocked() && draft == store_.current() ? true : store_.commit(draft);
    status_->setProperty("failed",!ok);
    status_->setText(ok ? QStringLiteral("已保存 · 设置打开时暂停轮盘") : store_.error());
    if(ok) for(int i=0;i<8;++i) slots_->item(i)->setText(QString::number(i+1)+"  "+(draft.slots[i].name.isEmpty()?QStringLiteral("空槽位"):draft.slots[i].name));
    if (ok) { setStyleSheet(settingsStyle(draft.theme)); preview_->preview(draft); }
}
}
