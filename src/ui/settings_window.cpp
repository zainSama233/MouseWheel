#include "ui/settings_window.h"
#include "ui/wheel_window.h"
#include "ui/theme.h"
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
namespace wheel {
SettingsWindow::SettingsWindow(ConfigStore& store) : store_(store) {
    setAttribute(Qt::WA_DeleteOnClose);
    setWindowTitle(QStringLiteral("鼠标快捷强化 · 设置"));
    setMinimumSize(640,480);
    resize(820,qMin(650,screen()->availableGeometry().height()-60));
    auto* root = new QVBoxLayout(this); root->setContentsMargins(0,0,0,0);
    auto* scroll = new QScrollArea; scroll->setWidgetResizable(true); scroll->setFrameShape(QFrame::NoFrame);
    auto* content = new QWidget; content->setMinimumWidth(780); scroll->setWidget(content); root->addWidget(scroll);
    auto* layout = new QVBoxLayout(content); layout->setContentsMargins(30,24,30,24); layout->setSpacing(18);
    auto* title = new QLabel(QStringLiteral("鼠标快捷强化")); title->setObjectName("title"); layout->addWidget(title);
    auto* toolbar = new QHBoxLayout;
    modifier_ = new QComboBox; button_ = new QComboBox; theme_ = new QComboBox;
    for (auto [name,value] : {std::pair{"Ctrl",Modifier::Control},{"Alt",Modifier::Alt},
                             {"Shift",Modifier::Shift},{"Win",Modifier::Meta}})
        modifier_->addItem(QString::fromLatin1(name),static_cast<int>(value));
    button_->addItems({QStringLiteral("鼠标右键"),QStringLiteral("鼠标中键"),QStringLiteral("侧键 · 后退"),QStringLiteral("侧键 · 前进")});
    theme_->addItems({QStringLiteral("明亮"),QStringLiteral("护眼"),QStringLiteral("深色")});
    toolbar->addWidget(new QLabel(QStringLiteral("按住"))); toolbar->addWidget(modifier_);
    toolbar->addWidget(new QLabel("+")); toolbar->addWidget(button_);
    toolbar->addStretch(); toolbar->addWidget(theme_); layout->addLayout(toolbar);
    auto* body = new QHBoxLayout;
    auto* grid = new QGridLayout; grid->setVerticalSpacing(10);
    grid->addWidget(new QLabel(QStringLiteral("槽位")),0,0);
    grid->addWidget(new QLabel(QStringLiteral("名称")),0,1);
    grid->addWidget(new QLabel(QStringLiteral("快捷键")),0,2);
    for (int i=0;i<8;++i) {
        auto* number = new QLabel(QString::number(i+1)); number->setObjectName("muted");
        names_[i] = new QLineEdit; names_[i]->setObjectName(QString("slot-name-%1").arg(i)); names_[i]->setMaxLength(12);
        names_[i]->setPlaceholderText(QStringLiteral("空槽位"));
        names_[i]->setAccessibleName(QStringLiteral("槽位 %1 名称").arg(i+1));
        shortcuts_[i] = new QKeySequenceEdit;
        shortcuts_[i]->setMaximumSequenceLength(1);
        shortcuts_[i]->setFinishingKeyCombinations({});
        shortcuts_[i]->setAccessibleName(QStringLiteral("槽位 %1 快捷键").arg(i+1));
        shortcuts_[i]->setToolTip(QStringLiteral("字母、数字、F1–F24、方向键或导航键"));
        auto* clear = new QPushButton(QStringLiteral("清空"));
        grid->addWidget(number,i+1,0); grid->addWidget(names_[i],i+1,1);
        grid->addWidget(shortcuts_[i],i+1,2); grid->addWidget(clear,i+1,3);
        connect(names_[i],&QLineEdit::editingFinished,this,&SettingsWindow::submit);
        connect(shortcuts_[i],&QKeySequenceEdit::editingFinished,this,&SettingsWindow::submit);
        connect(clear,&QPushButton::clicked,this,[this,i]{
            names_[i]->clear(); shortcuts_[i]->clear(); submit();
        });
    }
    body->addLayout(grid,1);
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
    theme_->setCurrentIndex(static_cast<int>(config.theme));
    for (int i=0;i<8;++i) {
        names_[i]->setText(config.slots[i].name);
        shortcuts_[i]->setKeySequence(QKeySequence(shortcutText(config.slots[i].shortcut),QKeySequence::NativeText));
    }
    setStyleSheet(settingsStyle(config.theme)); preview_->preview(config);
    status_->setText(store_.blocked() ? store_.error() : QStringLiteral("自动保存 · 设置打开时暂停轮盘"));
    status_->setProperty("failed",store_.blocked());
    populating_ = false;
}
void SettingsWindow::submit() {
    if (populating_) return;
    Config draft;
    draft.modifier = static_cast<Modifier>(modifier_->currentData().toInt());
    draft.button = static_cast<MouseButton>(button_->currentIndex());
    draft.theme = static_cast<Theme>(theme_->currentIndex());
    for (int i=0;i<8;++i) {
        const auto sequence = shortcuts_[i]->keySequence();
        Shortcut shortcut;
        if (!sequence.isEmpty()) {
            const auto combination = sequence[0];
            shortcut.key = combination.key();
            const auto mods = combination.keyboardModifiers();
            if (mods & Qt::ControlModifier) shortcut.modifiers |= bit(Modifier::Control);
            if (mods & Qt::AltModifier) shortcut.modifiers |= bit(Modifier::Alt);
            if (mods & Qt::ShiftModifier) shortcut.modifiers |= bit(Modifier::Shift);
            if (mods & Qt::MetaModifier) shortcut.modifiers |= bit(Modifier::Meta);
            if (mods & Qt::KeypadModifier) {
                status_->setText(QStringLiteral("请使用主键盘录制快捷键。")); status_->setProperty("failed",true); return;
            }
        }
        draft.slots[i] = {names_[i]->text().trimmed(),shortcut};
    }
    const bool ok = !store_.blocked() && draft == store_.current() ? true : store_.commit(draft);
    status_->setProperty("failed",!ok);
    status_->setText(ok ? QStringLiteral("已保存 · 设置打开时暂停轮盘") : store_.error());
    if (ok) { setStyleSheet(settingsStyle(draft.theme)); preview_->preview(draft); }
}
}
