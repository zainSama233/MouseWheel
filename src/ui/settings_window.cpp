#include <QCoreApplication>
#include "ui/settings_window.h"
#include "ui/settings_page.h"
#include "ui/content_stack.h"
#include <QButtonGroup>
#include <QToolButton>
#include <QFormLayout>
#include "ui/profile_panel.h"
#include "ui/style_editor.h"
#include "ui/localization.h"
#include <QCheckBox>
#include <QDoubleSpinBox>
#include "ui/wheel_window.h"
#include "ui/slot_editor.h"
#include "ui/trigger_rules_editor.h"
#include <QStackedWidget>
#include <QListWidget>
#include "ui/theme.h"
#include <QComboBox>
#include <QLabel>
#include <QPushButton>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QMessageBox>
#include <QScreen>
#include <QSignalBlocker>
#include <QStyleHints>
namespace wheel {
SettingsWindow::SettingsWindow(ConfigStore& store) : store_(store) {
    Localization::instance().setLanguage(store.current().language);
    setAttribute(Qt::WA_DeleteOnClose);
    setWindowTitle(QCoreApplication::translate("MouseWheel","鼠标快捷强化 · 设置"));
    setMinimumSize(900,580);
    resize(1060,qMin(780,screen()->availableGeometry().height()-60));
    auto* root=new QVBoxLayout(this);root->setContentsMargins(24,20,24,16);root->setSpacing(16);
    auto* heading=new QHBoxLayout;auto* title=new QLabel(QCoreApplication::translate("MouseWheel","鼠标快捷强化"));title->setObjectName("title");
    heading->addWidget(title);heading->addStretch();root->addLayout(heading);
    auto* advanced=new QCheckBox(QCoreApplication::translate("MouseWheel","高级设置"));advanced->setObjectName("advanced-settings");heading->addWidget(advanced);
    auto* tabs=new QHBoxLayout;auto* sections=new QButtonGroup(this);sections->setExclusive(true);
    const QStringList titles{QCoreApplication::translate("MouseWheel","动作配置"),QCoreApplication::translate("MouseWheel","轮盘外观"),QCoreApplication::translate("MouseWheel","通用设置")};
    const QStringList names{"settings-actions","settings-appearance","settings-general"};
    for(int i=0;i<titles.size();++i){auto* tab=new QPushButton(titles[i]);tab->setObjectName(names[i]);tab->setProperty("navigation",true);tab->setCheckable(true);sections->addButton(tab,i);tabs->addWidget(tab);}
    sections->button(0)->setChecked(true);tabs->addStretch();root->addLayout(tabs);
    profiles_=new ProfilePanel(store_,[this]{return pageComplete();});root->addWidget(profiles_);
    auto* workspace=new QHBoxLayout;workspace->setSpacing(24);root->addLayout(workspace,1);
    auto* sectionPages=new QStackedWidget;workspace->addWidget(sectionPages,1);
    auto* actionPage=new SettingsPage;auto* appearancePage=new SettingsPage;auto* generalPage=new SettingsPage;generalPage->widget()->setMaximumWidth(760);
    for(auto* page:{actionPage,appearancePage,generalPage})sectionPages->addWidget(page);
    auto* layout=actionPage->body();auto* appearanceLayout=appearancePage->body();auto* generalLayout=generalPage->body();
    generalPage->addSection(QCoreApplication::translate("MouseWheel","触发方式"));
    auto* toolbar = new QHBoxLayout;
    modifier_ = new QComboBox; button_ = new QComboBox; theme_ = new QComboBox; theme_->setObjectName("theme");
    modifier_->addItem(QCoreApplication::translate("MouseWheel","无修饰键"),static_cast<int>(Modifier::None));
    modifier_->setObjectName("trigger-modifier"); button_->setObjectName("trigger-button");
    for (auto [name,value] : {std::pair{"Ctrl",Modifier::Control},{"Alt",Modifier::Alt},
                             {"Shift",Modifier::Shift},{"Win",Modifier::Meta}})
        modifier_->addItem(QString::fromLatin1(name),static_cast<int>(value));
    button_->addItems({QCoreApplication::translate("MouseWheel","鼠标右键"),QCoreApplication::translate("MouseWheel","鼠标中键"),QCoreApplication::translate("MouseWheel","侧键 · 后退"),QCoreApplication::translate("MouseWheel","侧键 · 前进")});
    theme_->addItems({QCoreApplication::translate("MouseWheel","明亮"),QCoreApplication::translate("MouseWheel","护眼"),QCoreApplication::translate("MouseWheel","深色"),QCoreApplication::translate("MouseWheel","跟随系统"),QCoreApplication::translate("MouseWheel","莫兰迪柔灰"),QCoreApplication::translate("MouseWheel","海洋蓝")});
    toolbar->addWidget(new QLabel(QCoreApplication::translate("MouseWheel","按住"))); toolbar->addWidget(modifier_);
    toolbar->addWidget(new QLabel("+")); toolbar->addWidget(button_);
    language_=new QComboBox;language_->setProperty("locale-user-items",true);language_->setObjectName("language");language_->addItems({QStringLiteral("简体中文"),QStringLiteral("繁體中文"),"English",QStringLiteral("日本語")});
    auto* languageRow=new QFormLayout;languageRow->addRow(QCoreApplication::translate("MouseWheel","界面语言"),language_);connect(language_,&QComboBox::currentIndexChanged,this,&SettingsWindow::submit);
    toolbar->addStretch();generalLayout->addLayout(toolbar);
    auto* triggerHint = new QLabel(QCoreApplication::translate("MouseWheel","单键模式占用中键点击；滚轮照常使用。暂停后恢复中键原功能。"));
    triggerHint->setWordWrap(true);triggerHint->setObjectName("muted"); generalLayout->addWidget(triggerHint);
    connect(profiles_,&ProfilePanel::selected,this,[this]{group_=-1;populate();});
    appearancePage->addSection(QCoreApplication::translate("MouseWheel","主题与形态"));
    auto* appearance=new QFormLayout;appearance->setVerticalSpacing(16);
    appearance->addRow(QCoreApplication::translate("MouseWheel","配色主题"),theme_);
    shape_=new QComboBox; shape_->setObjectName("wheel-shape");
    shape_->addItem(QCoreApplication::translate("MouseWheel","经典紧凑扇区 · Original"),int(WheelShape::Original));
    shape_->addItem(QCoreApplication::translate("MouseWheel","独立悬浮圆形 · Circle"),int(WheelShape::Circle));
    shape_->addItem(QCoreApplication::translate("MouseWheel","圆角胶囊 · Capsule"),int(WheelShape::Capsule));
    shape_->addItem(QCoreApplication::translate("MouseWheel","蜂巢六边形 · HexagonHive"),int(WheelShape::HexagonHive));
    appearance->addRow(QCoreApplication::translate("MouseWheel","轮盘形态"),shape_);
    count_=new QComboBox;count_->setObjectName("wheel-count");
    for(int count:{4,8,12}) count_->addItem(QCoreApplication::translate("MouseWheel","%1 槽位").arg(count),count);
    appearanceLayout->addLayout(appearance);
    connect(shape_,&QComboBox::currentIndexChanged,this,&SettingsWindow::submit);
    generalPage->addSection(QCoreApplication::translate("MouseWheel","暂停规则"));
    rules_=new TriggerRulesEditor; generalLayout->addWidget(rules_); connect(rules_,&TriggerRulesEditor::edited,this,&SettingsWindow::submit);
    generalPage->addSection(QCoreApplication::translate("MouseWheel","语言与配置"));generalLayout->addLayout(languageRow);
    auto* precision=new QWidget;precision->hide();
    auto* behavior=new QFormLayout(precision);behavior->setContentsMargins(0,0,0,0);behavior->setVerticalSpacing(16);
    centerEnabled_=new QCheckBox(QCoreApplication::translate("MouseWheel","启用主中心动作"));centerEnabled_->setObjectName("center-enabled");layout->addWidget(centerEnabled_);
    deadZone_=new QDoubleSpinBox;deadZone_->setObjectName("center-dead-zone");deadZone_->setRange(12,52);deadZone_->setSuffix(" px");behavior->addRow(QCoreApplication::translate("MouseWheel","中心死区"),deadZone_);
    marginX_=new QDoubleSpinBox;marginY_=new QDoubleSpinBox;int axis=0;
    for(auto* input:{marginX_,marginY_}) {input->setRange(0,300);input->setSuffix(" px");input->setObjectName(axis==0?"safe-margin-x":"safe-margin-y");behavior->addRow(QCoreApplication::translate("MouseWheel",axis==0?"水平安全边距":"垂直安全边距"),input);++axis;connect(input,&QDoubleSpinBox::valueChanged,this,&SettingsWindow::submit);}
    edgePolicy_=new QComboBox;edgePolicy_->addItems({QCoreApplication::translate("MouseWheel","边缘平移"),QCoreApplication::translate("MouseWheel","边缘缩小")});behavior->addRow(QCoreApplication::translate("MouseWheel","屏幕边缘"),edgePolicy_);
    connect(centerEnabled_,&QCheckBox::toggled,this,&SettingsWindow::submit);connect(deadZone_,&QDoubleSpinBox::valueChanged,this,&SettingsWindow::submit);connect(edgePolicy_,&QComboBox::currentIndexChanged,this,&SettingsWindow::submit);
    frosted_=new QCheckBox(QCoreApplication::translate("MouseWheel","毛玻璃材质"));appearanceLayout->addWidget(frosted_);appearanceLayout->addWidget(precision);connect(frosted_,&QCheckBox::toggled,this,&SettingsWindow::submit);
    styleEditor_=new StyleEditor;styleEditor_->setObjectName("wheel-style");styleEditor_->hide();styleEditor_->setStore(&store_);appearanceLayout->addWidget(styleEditor_);connect(styleEditor_,&StyleEditor::edited,this,&SettingsWindow::submit);
    navigation_=new QComboBox;navigation_->setProperty("locale-user-items",true);navigation_->setObjectName("wheel-navigation");auto* levelBar=new QHBoxLayout;levelBar->addWidget(navigation_,1);levelBar->addWidget(count_);layout->insertLayout(0,levelBar);
    auto* body = new QHBoxLayout;
    auto* slots=new QListWidget; slots_=slots; slots->setFrameShape(QFrame::NoFrame); slots->setSpacing(5); slots->setObjectName("slot-list"); slots->setFixedHeight(180);slots->hide();
    auto* pages=new ContentStack;pages_=pages;
    for(int i=0;i<13;++i) {
        slots->addItem(QCoreApplication::translate("MouseWheel","槽位 %1").arg(i+1));
        editors_[i]=new SlotEditor(i);editors_[i]->setStore(&store_); pages->addWidget(editors_[i]);
        connect(editors_[i],&SlotEditor::edited,this,&SettingsWindow::submit);
        connect(editors_[i],&SlotEditor::editGroup,this,[this,i]{
            if(i==12 || group_>=0 || !pageComplete()) return;
            group_=i;populate();slots_->setCurrentRow(0);
        });
    }
    connect(advanced,&QCheckBox::toggled,this,[this,precision](bool enabled){
        precision->setVisible(enabled);styleEditor_->setVisible(enabled);
        for(auto* editor:editors_)editor->setAdvancedSettingsVisible(enabled);
    });
    connect(slots,&QListWidget::currentRowChanged,pages,&QStackedWidget::setCurrentIndex); slots->setCurrentRow(0);
    body->addWidget(pages,1);
    auto* sideHost=new SettingsPage;sideHost->setFixedWidth(340);workspace->addWidget(sideHost);
    auto* side=sideHost->body();side->setSpacing(12);
    auto* previewTitle=new QLabel(QCoreApplication::translate("MouseWheel","实时预览"));previewTitle->setObjectName("section");side->addWidget(previewTitle);
    preview_ = new WheelWindow(false);preview_->setAssetDirectory(store_.assetDirectory()); preview_->setFixedSize(300,300);
    preview_->setObjectName("wheel-preview");
    connect(preview_,&WheelWindow::slotClicked,this,[this,sections](int index){sections->button(0)->click();slots_->setCurrentRow(index==-2?12:index);});
    connect(slots,&QListWidget::currentRowChanged,this,[this](int index){preview_->select(0,index==12?-2:index);});
    connect(preview_,&WheelWindow::slotsSwapped,this,[this](int source,int target){
        source=source==-2?12:source;target=target==-2?12:target;
        if(source<0 || target<0 || source>12 || target>12)return;
        const auto first=editors_[source]->slot(),second=editors_[target]->slot();
        if((source==12 && second.kind()==ActionKind::Group) || (target==12 && first.kind()==ActionKind::Group)) {status_->setText(QCoreApplication::translate("MouseWheel","中心不能放置子轮盘"));return;}
        const auto error=validate(first).isEmpty()?validate(second):validate(first);
        if(!error.isEmpty()) {status_->setText(QCoreApplication::translate("MouseWheel","请先填写交换槽位：")+error); return;}
        editors_[source]->setSlot(second); editors_[target]->setSlot(first); submit();
        if(status_->property("failed").toBool()) {editors_[source]->setSlot(first); editors_[target]->setSlot(second);}
        else slots_->setCurrentRow(target);
    });
    connect(preview_,&WheelWindow::levelRequested,this,[this](int group){if(!pageComplete())return;group_=group;populate();});
    connect(preview_,&WheelWindow::positionsSwapped,this,[this](int fromGroup,int from,int toGroup,int to){
        if(!pageComplete())return;Config draft=store_.current();auto& current=wheel(draft);
        const auto position=[&](int group,int index)->Slot& {if(index==-2)return current.center;return group<0?current.slots[index]:std::get<GroupAction>(current.slots[group].action).slots[index];};
        auto& first=position(fromGroup,from);auto& second=position(toGroup,to);
        if((first.kind()==ActionKind::Group && (toGroup>=0 || to==-2)) || (second.kind()==ActionKind::Group && (fromGroup>=0 || from==-2))) {status_->setText(QCoreApplication::translate("MouseWheel","分组仅能放在主轮盘外圈"));return;}
        std::swap(first,second);if(!store_.commit(draft)){status_->setText(store_.error());return;}populate();slots_->setCurrentRow(to==-2?12:to);
    });
    side->addWidget(preview_,0,Qt::AlignCenter);
    auto* resetView=new QPushButton(QCoreApplication::translate("MouseWheel","复位视角"));side->addWidget(resetView);connect(resetView,&QPushButton::clicked,preview_,&WheelWindow::resetView);
    auto* overview=new QToolButton;overview->setObjectName("action-overview");overview->setText(QCoreApplication::translate("MouseWheel","动作全览"));overview->setCheckable(true);overview->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);overview->setArrowType(Qt::RightArrow);side->addWidget(overview);
    connect(overview,&QToolButton::toggled,this,[slots,overview](bool visible){slots->setVisible(visible);overview->setArrowType(visible?Qt::DownArrow:Qt::RightArrow);});
    side->addWidget(slots);
    auto* hint = new QLabel(QCoreApplication::translate("MouseWheel","点击编辑 · 拖拽交换位置")); hint->setObjectName("muted");
    hint->setWordWrap(true);hint->setAlignment(Qt::AlignCenter);hint->setMinimumHeight(32);side->insertWidget(2,hint);
    layout->addLayout(body);
    status_ = new QLabel; status_->setWordWrap(true); status_->setObjectName("save-status");root->addWidget(status_);
    auto* footer = new QHBoxLayout;
    auto* location = new QLabel(QCoreApplication::translate("MouseWheel","配置位置")); location->setToolTip(store_.path()); location->setObjectName("muted");
    footer->addWidget(location); footer->addStretch();
    auto* reset = new QPushButton(QCoreApplication::translate("MouseWheel","恢复默认"));
    footer->addWidget(reset);generalLayout->addLayout(footer);
    connect(sections,&QButtonGroup::idClicked,this,[this,sectionPages,sideHost,advanced](int index){sectionPages->setCurrentIndex(index);sideHost->setVisible(index!=2);profiles_->setVisible(index!=2);advanced->setVisible(index!=2);});
    connect(reset,&QPushButton::clicked,this,[this]{
        if (QMessageBox::question(this,QCoreApplication::translate("MouseWheel","恢复默认"),
            QCoreApplication::translate("MouseWheel","用默认配置替换当前配置文件？")) != QMessageBox::Yes) return;
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
            status_->setText(QCoreApplication::translate("MouseWheel","请先移动或清空第 %1 个及之后的已配置槽位，再减少数量。").arg(count+1));restore();return;
        }
        entries.resize(count);
        if(!store_.commit(draft)) {status_->setText(store_.error());restore();return;}
        populate();
    });
    connect(QGuiApplication::styleHints(),&QStyleHints::colorSchemeChanged,this,[this]{Config current=store_.current();setStyleSheet(settingsStyle(wheel(current).theme));});
    populate();
}
WheelConfig& SettingsWindow::wheel(Config& config) const {
    for(auto& profile:config.profiles)if(profile.id==profiles_->currentId())return profile.wheel;
    return config;
}
QList<Slot>& SettingsWindow::page(Config& config) const {
    auto& current=wheel(config);
    return group_<0?current.slots:std::get<GroupAction>(current.slots[group_].action).slots;
}
bool SettingsWindow::pageComplete() {
    Config config=store_.current();const int count=page(config).size();
    for(int i=0;i<count;++i) if(!validate(editors_[i]->slot()).isEmpty()) {
        status_->setText(QCoreApplication::translate("MouseWheel","请先完成或清空槽位 %1，再切换轮盘或数量。").arg(i+1));return false;
    }
    if(group_<0 && !validate(editors_[12]->slot()).isEmpty()) {status_->setText(QCoreApplication::translate("MouseWheel","请先完成或清空中心动作"));return false;}
    submit();return !status_->property("failed").toBool();
}
void SettingsWindow::populate() {
    populating_ = true;
    auto config = store_.current();auto& current=wheel(config);
    if(group_>=current.slots.size() || (group_>=0 && current.slots[group_].kind()!=ActionKind::Group)) group_=-1;
    auto& entries=page(config);
    language_->setCurrentIndex(int(config.language));
    rules_->setRules(config.triggerRules);
    modifier_->setCurrentIndex(modifier_->findData(static_cast<int>(config.modifier)));
    button_->setCurrentIndex(static_cast<int>(config.button));
    button_->setEnabled(config.modifier != Modifier::None);
    theme_->setCurrentIndex(static_cast<int>(current.theme));
    shape_->setCurrentIndex(shape_->findData(int(current.shape)));centerImage_=current.centerImage;
    styleEditor_->setStyle(current.style);frosted_->setChecked(current.frosted);
    centerEnabled_->setChecked(current.centerEnabled);deadZone_->setValue(current.deadZone);
    marginX_->setValue(current.safetyMargin.x());marginY_->setValue(current.safetyMargin.y());edgePolicy_->setCurrentIndex(int(current.edgePolicy));
    editors_[12]->setGroupsAllowed(false);editors_[12]->setSlot(current.center);slots_->item(12)->setText(QCoreApplication::translate("MouseWheel","中心动作"));slots_->item(12)->setHidden(group_>=0);
    count_->setCurrentIndex(count_->findData(entries.size()));
    navigation_->clear();navigation_->addItem(QCoreApplication::translate("MouseWheel","主轮盘"),-1);
    for(int i=0;i<current.slots.size();++i) if(current.slots[i].kind()==ActionKind::Group)
        navigation_->addItem(QCoreApplication::translate("MouseWheel","主轮盘 / ")+current.slots[i].name,i);
    navigation_->setCurrentIndex(navigation_->findData(group_));
    for(int i=0;i<12;++i) {
        slots_->item(i)->setHidden(i>=entries.size());
        editors_[i]->setGroupsAllowed(group_<0);
        editors_[i]->setSlot(i<entries.size()?entries[i]:Slot{});
        if(i<entries.size()) slots_->item(i)->setText(QString::number(i+1)+"  "+(entries[i].name.isEmpty()?QCoreApplication::translate("MouseWheel","空槽位"):entries[i].name)+(entries[i].kind()==ActionKind::Group?QStringLiteral("  ›"):QString{}));
    }
    if(slots_->currentRow()<0 || slots_->currentRow()>=entries.size()) slots_->setCurrentRow(0);
    setStyleSheet(settingsStyle(current.theme));
    Config view=config;static_cast<WheelConfig&>(view)=current;preview_->preview(view,group_); preview_->select(0,slots_->currentRow()==12?-2:slots_->currentRow());
    status_->setText(store_.blocked() ? store_.error() : QCoreApplication::translate("MouseWheel","自动保存 · 设置打开时暂停轮盘"));
    status_->setProperty("failed",store_.blocked());
    populating_ = false;
}
void SettingsWindow::submit() {
    if (populating_) return;
    Config draft=store_.current();draft.language=Language(language_->currentIndex());auto& current=wheel(draft);draft.triggerRules=rules_->rules();
    draft.modifier = static_cast<Modifier>(modifier_->currentData().toInt());
    if (draft.modifier == Modifier::None) {
        const QSignalBlocker blocker(button_);button_->setCurrentIndex(static_cast<int>(MouseButton::Middle));
    }
    button_->setEnabled(draft.modifier != Modifier::None);
    draft.button = static_cast<MouseButton>(button_->currentIndex());
    current.theme = static_cast<Theme>(theme_->currentIndex());
    current.shape=static_cast<WheelShape>(shape_->currentData().toInt()); current.centerImage=centerImage_;
    current.style=styleEditor_->style();current.frosted=frosted_->isChecked();
    current.centerEnabled=centerEnabled_->isChecked();current.deadZone=deadZone_->value();current.safetyMargin={marginX_->value(),marginY_->value()};current.edgePolicy=EdgePolicy(edgePolicy_->currentIndex());
    auto& entries=page(draft);QStringList unfinished;
    if(group_<0) {const auto center=editors_[12]->slot();const auto error=validate(center);if(error.isEmpty())current.center=center;else unfinished.append(QCoreApplication::translate("MouseWheel","中心：")+error);}
    for(int i=0;i<entries.size();++i) {
        const auto slot=editors_[i]->slot();const auto error=validate(slot);
        if(error.isEmpty()) entries[i]=slot;
        else unfinished.append(QCoreApplication::translate("MouseWheel","槽位 %1：%2").arg(i+1).arg(error));
    }
    Config saved=store_.current();const bool themeChanged=current.theme!=wheel(saved).theme;
    const bool ok = !store_.blocked() && draft == store_.current() ? true : store_.commit(draft);
    status_->setProperty("failed",!ok);
    status_->setText(!ok?store_.error():unfinished.isEmpty()?QCoreApplication::translate("MouseWheel","自动保存 · 关闭设置后恢复轮盘"):QCoreApplication::translate("MouseWheel","其他改动已保存；以下槽位未填完整：\n")+unfinished.join("\n"));
    if(ok) {
        Localization::instance().setLanguage(draft.language);
        {const QSignalBlocker blocker(count_);for(int i=0;i<count_->count();++i)count_->setItemText(i,QCoreApplication::translate("MouseWheel","%1 槽位").arg(count_->itemData(i).toInt()));}
        slots_->item(12)->setText(QCoreApplication::translate("MouseWheel","中心动作"));
        for(int i=0;i<entries.size();++i) slots_->item(i)->setText(QString::number(i+1)+"  "+(entries[i].name.isEmpty()?QCoreApplication::translate("MouseWheel","空槽位"):entries[i].name)+(entries[i].kind()==ActionKind::Group?QStringLiteral("  ›"):QString{}));
        const QSignalBlocker blocker(navigation_);navigation_->clear();navigation_->addItem(QCoreApplication::translate("MouseWheel","主轮盘"),-1);
        for(int i=0;i<current.slots.size();++i) if(current.slots[i].kind()==ActionKind::Group) navigation_->addItem(QCoreApplication::translate("MouseWheel","主轮盘 / ")+current.slots[i].name,i);
        navigation_->setCurrentIndex(navigation_->findData(group_));
        if(themeChanged) setStyleSheet(settingsStyle(current.theme));
        Config view=draft;static_cast<WheelConfig&>(view)=current;preview_->preview(view,group_);
    }
}
}
