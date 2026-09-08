#include "ui/settings_window.h"
#include "ui/wheel_window.h"
#include "ui/slot_editor.h"
#include "ui/trigger_rules_editor.h"
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
    modifier_ = new QComboBox; button_ = new QComboBox; theme_ = new QComboBox; theme_->setObjectName("theme");
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
    shape_->addItem(QStringLiteral("经典紧凑扇区 · Original"),int(WheelShape::Original));
    shape_->addItem(QStringLiteral("独立悬浮圆形 · Circle"),int(WheelShape::Circle));
    shape_->addItem(QStringLiteral("圆角胶囊 · Capsule"),int(WheelShape::Capsule));
    shape_->addItem(QStringLiteral("蜂巢六边形 · HexagonHive"),int(WheelShape::HexagonHive));
    appearance->addWidget(shape_);
    count_=new QComboBox;count_->setObjectName("wheel-count");
    for(int count:{4,8,12}) count_->addItem(QStringLiteral("%1 槽位").arg(count),count);
    appearance->addWidget(count_);
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
    rules_=new TriggerRulesEditor; layout->addWidget(rules_); connect(rules_,&TriggerRulesEditor::edited,this,&SettingsWindow::submit);
    navigation_=new QComboBox;navigation_->setObjectName("wheel-navigation");layout->addWidget(navigation_);
    auto* body = new QHBoxLayout;
    auto* slots=new QListWidget; slots_=slots; slots->setFrameShape(QFrame::NoFrame); slots->setSpacing(5); slots->setObjectName("slot-list"); slots->setMinimumHeight(170);
    auto* pages=new QStackedWidget;pages_=pages;
    for(int i=0;i<12;++i) {
        slots->addItem(QStringLiteral("槽位 %1").arg(i+1));
        editors_[i]=new SlotEditor(i); pages->addWidget(editors_[i]);
        connect(editors_[i],&SlotEditor::edited,this,&SettingsWindow::submit);
        connect(editors_[i],&SlotEditor::editGroup,this,[this,i]{
            if(group_>=0 || !pageComplete()) return;
            group_=i;populate();slots_->setCurrentRow(0);
        });
    }
    connect(slots,&QListWidget::currentRowChanged,pages,&QStackedWidget::setCurrentIndex); slots->setCurrentRow(0);
    body->addWidget(pages,1);
    auto* side = new QVBoxLayout;
    preview_ = new WheelWindow(false); preview_->setFixedSize(264,264);
    preview_->setObjectName("wheel-preview");
    connect(preview_,&WheelWindow::slotClicked,slots,qOverload<int>(&QListWidget::setCurrentRow));
    connect(slots,&QListWidget::currentRowChanged,this,[this](int index){preview_->select(0,index);});
    connect(preview_,&WheelWindow::slotsSwapped,this,[this](int source,int target){
        const auto first=editors_[source]->slot(),second=editors_[target]->slot();
        const auto error=validate(first).isEmpty()?validate(second):validate(first);
        if(!error.isEmpty()) {status_->setText(QStringLiteral("请先填写交换槽位：")+error); return;}
        editors_[source]->setSlot(second); editors_[target]->setSlot(first); submit();
        if(status_->property("failed").toBool()) {editors_[source]->setSlot(first); editors_[target]->setSlot(second);}
        else slots_->setCurrentRow(target);
    });
    side->addWidget(preview_,0,Qt::AlignCenter);side->addWidget(slots);
    auto* hint = new QLabel(QStringLiteral("点击编辑 · 拖拽交换位置")); hint->setObjectName("muted");
    side->addWidget(hint,0,Qt::AlignCenter); side->addStretch();
    body->insertLayout(0,side); layout->addLayout(body,1);
    status_ = new QLabel; status_->setWordWrap(true); status_->setObjectName("muted"); layout->addWidget(status_);
    auto* footer = new QHBoxLayout;
    auto* location = new QLabel(QStringLiteral("配置位置")); location->setToolTip(store_.path()); location->setObjectName("muted");
    footer->addWidget(location); footer->addStretch();
    auto* reset = new QPushButton(QStringLiteral("恢复默认"));
    footer->addWidget(reset); layout->addLayout(footer);
    connect(reset,&QPushButton::clicked,this,[this]{
        if (QMessageBox::question(this,QStringLiteral("恢复默认"),
            QStringLiteral("用默认配置替换当前配置文件？")) != QMessageBox::Yes) return;
        if (store_.reset()) {group_=-1;populate();} else status_->setText(store_.error());
    });
    connect(modifier_,&QComboBox::currentIndexChanged,this,&SettingsWindow::submit);
    connect(button_,&QComboBox::currentIndexChanged,this,&SettingsWindow::submit);
    connect(theme_,&QComboBox::currentIndexChanged,this,&SettingsWindow::submit);
    connect(navigation_,&QComboBox::currentIndexChanged,this,[this]{
        if(populating_) return;
        const int next=navigation_->currentData().toInt();
        if(!pageComplete()) {const QSignalBlocker blocker(navigation_);navigation_->setCurrentIndex(navigation_->findData(group_));return;}
        group_=next;populate();slots_->setCurrentRow(0);
    });
    connect(count_,&QComboBox::currentIndexChanged,this,[this]{
        if(populating_) return;
        Config draft=store_.current();auto& entries=page(draft);const int count=count_->currentData().toInt();
        const int savedCount=entries.size();
        const auto restore=[this,savedCount]{const QSignalBlocker blocker(count_);count_->setCurrentIndex(count_->findData(savedCount));};
        if(!pageComplete()) {restore();return;}
        for(int i=count;i<entries.size();++i) if(entries[i].enabled()) {
            status_->setText(QStringLiteral("请先移动或清空第 %1 个及之后的已配置槽位，再减少数量。").arg(count+1));restore();return;
        }
        entries.resize(count);
        if(!store_.commit(draft)) {status_->setText(store_.error());restore();return;}
        populate();
    });
    populate();
}
QList<Slot>& SettingsWindow::page(Config& config) const {
    return group_<0?config.slots:std::get<GroupAction>(config.slots[group_].action).slots;
}
bool SettingsWindow::pageComplete() {
    Config config=store_.current();const int count=page(config).size();
    for(int i=0;i<count;++i) if(!validate(editors_[i]->slot()).isEmpty()) {
        status_->setText(QStringLiteral("请先完成或清空槽位 %1，再切换轮盘或数量。").arg(i+1));return false;
    }
    submit();return !status_->property("failed").toBool();
}
void SettingsWindow::populate() {
    populating_ = true;
    auto config = store_.current();
    if(group_>=config.slots.size() || (group_>=0 && config.slots[group_].kind()!=ActionKind::Group)) group_=-1;
    auto& entries=page(config);
    rules_->setRules(config.triggerRules);
    modifier_->setCurrentIndex(modifier_->findData(static_cast<int>(config.modifier)));
    button_->setCurrentIndex(static_cast<int>(config.button));
    button_->setEnabled(config.modifier != Modifier::None);
    theme_->setCurrentIndex(static_cast<int>(config.theme));
    shape_->setCurrentIndex(shape_->findData(int(config.shape)));centerImage_=config.centerImage;
    count_->setCurrentIndex(count_->findData(entries.size()));
    navigation_->clear();navigation_->addItem(QStringLiteral("主轮盘"),-1);
    for(int i=0;i<config.slots.size();++i) if(config.slots[i].kind()==ActionKind::Group)
        navigation_->addItem(QStringLiteral("主轮盘 / ")+config.slots[i].name,i);
    navigation_->setCurrentIndex(navigation_->findData(group_));
    for(int i=0;i<12;++i) {
        slots_->item(i)->setHidden(i>=entries.size());
        editors_[i]->setGroupsAllowed(group_<0);
        editors_[i]->setSlot(i<entries.size()?entries[i]:Slot{});
        if(i<entries.size()) slots_->item(i)->setText(QString::number(i+1)+"  "+(entries[i].name.isEmpty()?QStringLiteral("空槽位"):entries[i].name)+(entries[i].kind()==ActionKind::Group?QStringLiteral("  ›"):QString{}));
    }
    if(slots_->currentRow()<0 || slots_->currentRow()>=entries.size()) slots_->setCurrentRow(0);
    setStyleSheet(settingsStyle(config.theme));
    preview_->preview(config,group_); preview_->select(0,slots_->currentRow());
    status_->setText(store_.blocked() ? store_.error() : QStringLiteral("自动保存 · 设置打开时暂停轮盘"));
    status_->setProperty("failed",store_.blocked());
    populating_ = false;
}
void SettingsWindow::submit() {
    if (populating_) return;
    Config draft=store_.current();draft.triggerRules=rules_->rules();
    draft.modifier = static_cast<Modifier>(modifier_->currentData().toInt());
    if (draft.modifier == Modifier::None) {
        const QSignalBlocker blocker(button_);button_->setCurrentIndex(static_cast<int>(MouseButton::Middle));
    }
    button_->setEnabled(draft.modifier != Modifier::None);
    draft.button = static_cast<MouseButton>(button_->currentIndex());
    draft.theme = static_cast<Theme>(theme_->currentIndex());
    draft.shape=static_cast<WheelShape>(shape_->currentData().toInt()); draft.centerImage=centerImage_;
    auto& entries=page(draft);QStringList unfinished;
    for(int i=0;i<entries.size();++i) {
        const auto slot=editors_[i]->slot();const auto error=validate(slot);
        if(error.isEmpty()) entries[i]=slot;
        else unfinished.append(QStringLiteral("槽位 %1：%2").arg(i+1).arg(error));
    }
    const bool themeChanged=draft.theme!=store_.current().theme;
    const bool ok = !store_.blocked() && draft == store_.current() ? true : store_.commit(draft);
    status_->setProperty("failed",!ok);
    status_->setText(!ok?store_.error():unfinished.isEmpty()?QStringLiteral("自动保存 · 关闭设置后恢复轮盘"):QStringLiteral("其他改动已保存；以下槽位未填完整：\n")+unfinished.join("\n"));
    if(ok) {
        for(int i=0;i<entries.size();++i) slots_->item(i)->setText(QString::number(i+1)+"  "+(entries[i].name.isEmpty()?QStringLiteral("空槽位"):entries[i].name)+(entries[i].kind()==ActionKind::Group?QStringLiteral("  ›"):QString{}));
        const QSignalBlocker blocker(navigation_);navigation_->clear();navigation_->addItem(QStringLiteral("主轮盘"),-1);
        for(int i=0;i<draft.slots.size();++i) if(draft.slots[i].kind()==ActionKind::Group) navigation_->addItem(QStringLiteral("主轮盘 / ")+draft.slots[i].name,i);
        navigation_->setCurrentIndex(navigation_->findData(group_));
        if(themeChanged) setStyleSheet(settingsStyle(draft.theme));
        preview_->preview(draft,group_);
    }
}
}
