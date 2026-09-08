#include <QCoreApplication>
#include "ui/shortcut_editor.h"
#include <QKeySequenceEdit>
#include <QComboBox>
#include <QCompleter>
#include <QCheckBox>
#include <QPushButton>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include "platform/shortcut_capture.h"
namespace wheel {
ShortcutEditor::ShortcutEditor(QWidget* parent):QWidget(parent) {
    auto* layout=new QVBoxLayout(this); layout->setContentsMargins(0,0,0,0);
    sequence_=new QKeySequenceEdit; sequence_->setMaximumSequenceLength(1); sequence_->setFinishingKeyCombinations({}); layout->addWidget(sequence_);
    auto* row=new QHBoxLayout; key_=new QComboBox; key_->setEditable(true); key_->setInsertPolicy(QComboBox::NoInsert);
    key_->addItem(QCoreApplication::translate("MouseWheel","搜索主按键"),0);
    for(int k=Qt::Key_0;k<=Qt::Key_Z;++k) if(supportedKey(k)) key_->addItem(shortcutText({k,0}),k);
    for(int k=Qt::Key_Escape;k<=Qt::Key_F24;++k) if(supportedKey(k)) key_->addItem(shortcutText({k,0}),k);
    key_->addItem("Break",int(Qt::Key_Cancel)); key_->addItem("Space",int(Qt::Key_Space)); key_->completer()->setFilterMode(Qt::MatchContains); key_->completer()->setCaseSensitivity(Qt::CaseInsensitive); row->addWidget(key_);
    const QStringList names{"Ctrl","Alt","Shift","Win"};
    for(int i=0;i<4;++i) { modifiers_[i]=new QCheckBox(names[i]); row->addWidget(modifiers_[i]); }
    layout->addLayout(row); record_=new QPushButton(QCoreApplication::translate("MouseWheel","独占录入")); layout->addWidget(record_);
    const auto assemble=[this]{if(loading_) return; Shortcut s{key_->currentData().toInt(),0}; for(int i=0;i<4;++i) if(modifiers_[i]->isChecked()) s.modifiers|=1u<<i; setShortcut(s); Q_EMIT edited();};
    connect(key_,&QComboBox::activated,this,assemble);
    for(auto* box:modifiers_) connect(box,&QCheckBox::clicked,this,assemble);
    connect(sequence_,&QKeySequenceEdit::editingFinished,this,[this]{if(!loading_) {setShortcut(shortcut()); Q_EMIT edited();}});
    connect(record_,&QPushButton::clicked,this,[this]{
        if(auto* capture=ShortcutCapture::active()) {capture->cancel();return;}
        auto* capture=new ShortcutCapture(this); capture_=capture;
        connect(capture,&ShortcutCapture::recorded,this,[this](Shortcut shortcut){setShortcut(shortcut);Q_EMIT edited();});
        connect(capture,&ShortcutCapture::stopped,this,[this]{record_->setText(QCoreApplication::translate("MouseWheel","独占录入"));});
        record_->setText(capture->start()?QCoreApplication::translate("MouseWheel","录入中 · 点击取消"):QCoreApplication::translate("MouseWheel","录入启动失败"));
    });
}
Shortcut ShortcutEditor::shortcut() const {
    const auto s=sequence_->keySequence(); if(s.isEmpty()) return {};
    const auto c=s[0]; Modifiers mods=0; const auto m=c.keyboardModifiers();
    if(m&Qt::ControlModifier) mods|=bit(Modifier::Control); if(m&Qt::AltModifier) mods|=bit(Modifier::Alt);
    if(m&Qt::ShiftModifier) mods|=bit(Modifier::Shift); if(m&Qt::MetaModifier) mods|=bit(Modifier::Meta);
    return {int(c.key()),mods};
}
void ShortcutEditor::hideEvent(QHideEvent* event) {if(capture_)capture_->cancel();QWidget::hideEvent(event);}
void ShortcutEditor::setShortcut(Shortcut value) {
    if(capture_)capture_->cancel();
    loading_=true; sequence_->setKeySequence(shortcutSequence(value));
    key_->setCurrentIndex(key_->findData(value.key)); for(int i=0;i<4;++i) modifiers_[i]->setChecked(value.modifiers&(1u<<i)); loading_=false;
}
}
