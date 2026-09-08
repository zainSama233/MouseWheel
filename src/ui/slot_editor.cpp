#include <QCoreApplication>
#include "ui/slot_editor.h"
#include "ui/style_editor.h"
#include "ui/application_picker.h"
#include "ui/icon_library_dialog.h"
#include "ui/shortcut_editor.h"
#include "ui/action_icons.h"
#include "tools/website_icon.h"
#include <QTimer>
#include "core/image_asset.h"
#include <QLineEdit>
#include <QComboBox>
#include <QCheckBox>
#include <QPlainTextEdit>
#include <QSpinBox>
#include <QStackedWidget>
#include <QFormLayout>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QPushButton>
#include <QFileDialog>
#include <QFileInfo>
#include <QLabel>
#include <QMessageBox>
#include <QUrl>
#include <QSignalBlocker>
#include <QMessageBox>
namespace wheel {
class ContentStack final:public QStackedWidget {
public:
    ContentStack() {
        setSizePolicy(QSizePolicy::Preferred,QSizePolicy::Maximum);
        connect(this,&QStackedWidget::currentChanged,this,[this]{updateGeometry();});
    }
    QSize sizeHint() const override {return currentWidget()?currentWidget()->sizeHint():QSize{};}
    QSize minimumSizeHint() const override {return currentWidget()?currentWidget()->minimumSizeHint():QSize{};}
};
SlotEditor::SlotEditor(int index,QWidget* parent):QWidget(parent) {
    setObjectName(QString("slot-editor-%1").arg(index));
    iconTimer_=new QTimer(this); iconTimer_->setSingleShot(true); iconTimer_->setInterval(350);
    connect(iconTimer_,&QTimer::timeout,this,&SlotEditor::refreshAutomaticIcon);
    auto* root=new QVBoxLayout(this); root->setContentsMargins(0,0,0,0);
    auto* head=new QFormLayout; root->addLayout(head);
    const auto changed=[this]{if(!loading_) Q_EMIT edited();};
    const auto line=[&](QFormLayout* form,const QString& label,const QString& object=QString{}) {
        auto* field=new QLineEdit; field->setObjectName(object); field->setMaxLength(8192); form->addRow(label,field);
        connect(field,&QLineEdit::textChanged,this,changed); return field;
    };
    const auto combo=[&](QFormLayout* form,const QString& label,const QStringList& entries) {
        auto* field=new QComboBox; field->addItems(entries); form->addRow(label,field); connect(field,&QComboBox::currentIndexChanged,this,changed); return field;
    };
    const auto browse=[&](QFormLayout* form,QLineEdit* field,bool directory) {
        auto* button=new QPushButton(QCoreApplication::translate("MouseWheel","选择…"));
        int row=0; QFormLayout::ItemRole role; form->getWidgetPosition(field,&row,&role); form->removeWidget(field);
        auto* controls=new QHBoxLayout; controls->addWidget(field,1); controls->addWidget(button);
        form->setLayout(row,QFormLayout::FieldRole,controls);
        connect(button,&QPushButton::clicked,this,[this,field,directory]{
            const auto path=directory?QFileDialog::getExistingDirectory(this,QCoreApplication::translate("MouseWheel","选择文件夹")):QFileDialog::getOpenFileName(this,QCoreApplication::translate("MouseWheel","选择文件"));
            if(path.isEmpty()) return;
 field->setText(path);
            if(field==appPath_ && (name_->text().isEmpty() || name_->text()==actionKindName(ActionKind::Application))) name_->setText(QFileInfo(path).completeBaseName().left(12));
            Q_EMIT edited();
        });
    };
    name_=line(head,QCoreApplication::translate("MouseWheel","名称"),QString("slot-name-%1").arg(index)); name_->setMaxLength(64);
    kind_=new QComboBox; kind_->setObjectName(QString("slot-kind-%1").arg(index));
    for(int i=0;i<=int(ActionKind::Group);++i) kind_->addItem(actionKindName(ActionKind(i)));
    head->addRow(QCoreApplication::translate("MouseWheel","动作"),kind_);
    pages_=new ContentStack; root->addWidget(pages_);
    std::array<QFormLayout*,11> forms{};
    for(auto& form:forms) { auto* page=new QWidget; form=new QFormLayout(page); form->setContentsMargins(0,0,0,0); pages_->addWidget(page); }
    shortcut_=new ShortcutEditor; forms[0]->addRow(shortcut_); connect(shortcut_,&ShortcutEditor::edited,this,[this]{if(name_->text().isEmpty()) name_->setText(shortcutText(shortcut_->shortcut()).left(12)); if(!loading_) Q_EMIT edited();});
    forms[1]->addRow(new QLabel(QCoreApplication::translate("MouseWheel","框选屏幕，生成独立贴图"))); forms[2]->addRow(new QLabel(QCoreApplication::translate("MouseWheel","直接在桌面绘制标注")));
    appPath_=line(forms[3],QCoreApplication::translate("MouseWheel","文件"),QString("slot-app-%1").arg(index)); browse(forms[3],appPath_,false);
    auto* findApp=new QPushButton(QCoreApplication::translate("MouseWheel","搜索应用 / 运行窗口…")); findApp->setObjectName(QString("slot-find-app-%1").arg(index)); forms[3]->addRow(findApp);
    connect(findApp,&QPushButton::clicked,this,[this]{
        auto* picker=new ApplicationPicker(ApplicationPicker::Purpose::Launch,this);
        connect(picker,&ApplicationPicker::chosen,this,[this](const ApplicationEntry& entry,bool useIcon){
            auto current=slot(); current.action=ApplicationAction{entry.path,{},{},normal_->isChecked()};
            if(current.name.isEmpty() || current.name==actionKindName(ActionKind::Application)) current.name=entry.name.left(12);
            if(useIcon) current.icon={IconSource::Program,entry.executable,{}};
            setSlot(current); Q_EMIT edited();
        }); picker->open();
    });
    arguments_=line(forms[3],QCoreApplication::translate("MouseWheel","参数")); directory_=line(forms[3],QCoreApplication::translate("MouseWheel","工作目录")); browse(forms[3],directory_,true);
    normal_=new QCheckBox(QCoreApplication::translate("MouseWheel","使用普通权限启动")); normal_->setChecked(true); forms[3]->addRow(normal_); connect(normal_,&QCheckBox::toggled,this,changed);
    url_=line(forms[4],QCoreApplication::translate("MouseWheel","网址"),QString("slot-target-%1").arg(index));
    browser_=combo(forms[4],QCoreApplication::translate("MouseWheel","浏览器"),{QCoreApplication::translate("MouseWheel","系统默认"),"Chrome","Edge","Firefox",QCoreApplication::translate("MouseWheel","自定义")});
    browserPath_=line(forms[4],QCoreApplication::translate("MouseWheel","自定义浏览器")); browse(forms[4],browserPath_,false);
    connect(url_,&QLineEdit::textChanged,this,[this]{
        if(loading_) return; if(websiteIcon_) websiteIcon_->cancel();
        if(iconSource_->currentIndex()==int(IconSource::Automatic)) iconTimer_->start();
    });
    connect(browser_,&QComboBox::currentIndexChanged,this,[this](int value){browserPath_->setEnabled(value==int(Browser::Custom));});
    folder_=combo(forms[5],QCoreApplication::translate("MouseWheel","目录"),{QCoreApplication::translate("MouseWheel","自定义路径"),QCoreApplication::translate("MouseWheel","桌面"),QCoreApplication::translate("MouseWheel","下载"),QCoreApplication::translate("MouseWheel","文档"),QCoreApplication::translate("MouseWheel","图片"),QCoreApplication::translate("MouseWheel","用户目录"),QCoreApplication::translate("MouseWheel","此电脑"),QCoreApplication::translate("MouseWheel","回收站")});
    folderPath_=line(forms[5],QCoreApplication::translate("MouseWheel","路径")); browse(forms[5],folderPath_,true);
    connect(folder_,&QComboBox::currentIndexChanged,this,[this](int value){folderPath_->setEnabled(value==0);});
    shell_=combo(forms[6],QCoreApplication::translate("MouseWheel","终端"),{"CMD","PowerShell","WSL"});
    script_=new QPlainTextEdit; script_->setMaximumHeight(110); forms[6]->addRow(QCoreApplication::translate("MouseWheel","命令"),script_); connect(script_,&QPlainTextEdit::textChanged,this,changed);
    commandDirectory_=line(forms[6],QCoreApplication::translate("MouseWheel","工作目录")); browse(forms[6],commandDirectory_,true);
    hidden_=new QCheckBox(QCoreApplication::translate("MouseWheel","隐藏终端")); forms[6]->addRow(hidden_); connect(hidden_,&QCheckBox::toggled,this,changed);
    provider_=combo(forms[7],QCoreApplication::translate("MouseWheel","识别方式"),{QCoreApplication::translate("MouseWheel","本地 Windows OCR"),QCoreApplication::translate("MouseWheel","AI · 兼容 Chat Completions"),QCoreApplication::translate("MouseWheel","自定义 HTTP")});
    endpoint_=line(forms[7],QCoreApplication::translate("MouseWheel","完整接口地址")); apiKey_=line(forms[7],QStringLiteral("API Key")); apiKey_->setEchoMode(QLineEdit::Password);
    model_=line(forms[7],QCoreApplication::translate("MouseWheel","模型")); resultPath_=line(forms[7],QCoreApplication::translate("MouseWheel","结果字段"));
    connect(provider_,&QComboBox::currentIndexChanged,this,[this](int value){endpoint_->setEnabled(value!=0);apiKey_->setEnabled(value!=0);model_->setEnabled(value==1);resultPath_->setEnabled(value==2);});
    window_=combo(forms[8],QCoreApplication::translate("MouseWheel","操作"),{QCoreApplication::translate("MouseWheel","切换窗口"),QCoreApplication::translate("MouseWheel","左半屏"),QCoreApplication::translate("MouseWheel","右半屏"),QCoreApplication::translate("MouseWheel","移到下一显示器"),QCoreApplication::translate("MouseWheel","切换置顶"),QCoreApplication::translate("MouseWheel","设置透明度"),QCoreApplication::translate("MouseWheel","最大化／还原"),QCoreApplication::translate("MouseWheel","最小化")});
    opacity_=new QSpinBox; opacity_->setRange(20,100); opacity_->setSuffix("%"); forms[8]->addRow(QCoreApplication::translate("MouseWheel","不透明度"),opacity_); connect(opacity_,&QSpinBox::valueChanged,this,changed);
    connect(window_,&QComboBox::currentIndexChanged,this,[this](int value){opacity_->setEnabled(value==int(WindowOperation::Opacity));});
    system_=combo(forms[9],QCoreApplication::translate("MouseWheel","操作"),{QCoreApplication::translate("MouseWheel","锁屏"),QCoreApplication::translate("MouseWheel","音量增加"),QCoreApplication::translate("MouseWheel","音量降低"),QCoreApplication::translate("MouseWheel","静音"),QCoreApplication::translate("MouseWheel","播放／暂停"),QCoreApplication::translate("MouseWheel","下一首"),QCoreApplication::translate("MouseWheel","上一首"),QCoreApplication::translate("MouseWheel","任务视图"),QCoreApplication::translate("MouseWheel","上一个虚拟桌面"),QCoreApplication::translate("MouseWheel","下一个虚拟桌面"),QCoreApplication::translate("MouseWheel","新建虚拟桌面"),QCoreApplication::translate("MouseWheel","关闭虚拟桌面"),QCoreApplication::translate("MouseWheel","显示桌面")});
    auto* appearance=new QFormLayout; root->addLayout(appearance);
    auto* editGroup=new QPushButton(QCoreApplication::translate("MouseWheel","编辑子轮盘"));editGroup->setObjectName(QString("slot-edit-group-%1").arg(index));
    forms[10]->addRow(editGroup);connect(editGroup,&QPushButton::clicked,this,&SlotEditor::editGroup);
    iconSource_=combo(appearance,QCoreApplication::translate("MouseWheel","图标来源"),{QCoreApplication::translate("MouseWheel","内置矢量图标"),QCoreApplication::translate("MouseWheel","程序图标"),QCoreApplication::translate("MouseWheel","自定义图片"),QCoreApplication::translate("MouseWheel","自动获取"),QCoreApplication::translate("MouseWheel","图标库")});
    iconSource_->setObjectName(QString("slot-icon-source-%1").arg(index)); icons_=new ContentStack; appearance->addRow(icons_);
    symbol_=new QComboBox; symbol_->setObjectName(QString("slot-symbol-%1").arg(index));
    for(const auto& icon:builtinIcons()) symbol_->addItem(symbolIcon(icon.id,QColor("#6579a8")),icon.title,icon.id); icons_->addWidget(symbol_); connect(symbol_,&QComboBox::currentIndexChanged,this,changed);
    auto* programPage=new QWidget; auto* programForm=new QFormLayout(programPage); programForm->setContentsMargins(0,0,0,0);
    iconProgram_=line(programForm,QCoreApplication::translate("MouseWheel","来源文件")); browse(programForm,iconProgram_,false); icons_->addWidget(programPage);
    auto* imageButton=new QPushButton(QCoreApplication::translate("MouseWheel","导入图片…")); icons_->addWidget(imageButton);
    connect(imageButton,&QPushButton::clicked,this,[this]{const auto path=QFileDialog::getOpenFileName(this,QCoreApplication::translate("MouseWheel","图标图片"),{},QCoreApplication::translate("MouseWheel","图标 (*.svg *.png *.ico *.jpg *.jpeg)")); if(path.isEmpty()) return;
        if(store_) {
            const auto id=store_->importIcon(path);
            if(id.isEmpty()) {QMessageBox::warning(this,QCoreApplication::translate("MouseWheel","导入失败"),store_->error());return;}
            libraryId_=id;iconSource_->setCurrentIndex(int(IconSource::Library));Q_EMIT edited();return;
        }
 QString error; QByteArray png; if(!importImageAsset(path,png,error)) {QMessageBox::warning(this,QCoreApplication::translate("MouseWheel","图片不可用"),error);return;} image_=png; Q_EMIT edited();});
    auto* automaticPage=new QWidget; auto* automaticLayout=new QHBoxLayout(automaticPage); automaticLayout->setContentsMargins(0,0,0,0);
    iconStatus_=new QLabel; iconStatus_->setWordWrap(true); automaticLayout->addWidget(iconStatus_,1);
    auto* fetchIcon=new QPushButton(QCoreApplication::translate("MouseWheel","获取网站图标")); fetchIcon_=fetchIcon; fetchIcon->setObjectName(QString("slot-fetch-icon-%1").arg(index)); automaticLayout->addWidget(fetchIcon); icons_->addWidget(automaticPage);
    connect(fetchIcon,&QPushButton::clicked,this,[this]{automaticUrl_.clear(); refreshAutomaticIcon();});
    connect(kind_,&QComboBox::currentIndexChanged,this,[fetchIcon](int kind){fetchIcon->setVisible(kind==int(ActionKind::Website));});
    auto* libraryPage=new QWidget;auto* libraryLayout=new QVBoxLayout(libraryPage);libraryButton_=new QPushButton(QCoreApplication::translate("MouseWheel","选择共享图标…"));libraryButton_->setObjectName(QString("slot-library-%1").arg(index));libraryButton_->setEnabled(false);libraryLayout->addWidget(libraryButton_);icons_->addWidget(libraryPage);
    connect(libraryButton_,&QPushButton::clicked,this,[this]{
        if(!store_)return;auto* dialog=new IconLibraryDialog(*store_,this);
        connect(dialog,&IconLibraryDialog::chosen,this,[this](const QString& id){libraryId_=id;iconSource_->setCurrentIndex(int(IconSource::Library));Q_EMIT edited();});dialog->open();
    });
    connect(iconSource_,&QComboBox::currentIndexChanged,icons_,&QStackedWidget::setCurrentIndex);
    connect(iconSource_,&QComboBox::currentIndexChanged,this,[this](int source){
        if(loading_) return;
        if(websiteIcon_) websiteIcon_->cancel(); iconTimer_->stop();
        if(source==int(IconSource::Automatic)) iconTimer_->start();
    });
    label_=new QCheckBox(QCoreApplication::translate("MouseWheel","显示名称")); appearance->addRow(label_); connect(label_,&QCheckBox::toggled,this,changed);
    styleEditor_=new StyleEditor;styleEditor_->hide();root->addWidget(styleEditor_);connect(styleEditor_,&StyleEditor::edited,this,&SlotEditor::edited);
    auto* clear=new QPushButton(QCoreApplication::translate("MouseWheel","清空槽位")); root->addWidget(clear); connect(clear,&QPushButton::clicked,this,[this]{
        if(slot().kind()==ActionKind::Group && std::any_of(group_.slots.begin(),group_.slots.end(),[](const Slot& s){return s.enabled();}) &&
           QMessageBox::question(this,QCoreApplication::translate("MouseWheel","清空分组"),QCoreApplication::translate("MouseWheel","清空此分组及其中所有动作？"))!=QMessageBox::Yes) return;
        setSlot({}); Q_EMIT edited();
    });
    connect(kind_,&QComboBox::currentIndexChanged,this,[this](int value){
        if(!loading_ && pages_->currentIndex()==int(ActionKind::Group) && value!=int(ActionKind::Group) &&
           std::any_of(group_.slots.begin(),group_.slots.end(),[](const Slot& s){return s.enabled();})) {
            const QSignalBlocker blocker(kind_);kind_->setCurrentIndex(int(ActionKind::Group));
            QMessageBox::information(this,QCoreApplication::translate("MouseWheel","保留分组动作"),QCoreApplication::translate("MouseWheel","请先编辑并清空子轮盘，再更换动作类型。"));return;
        }
        pages_->setCurrentIndex(value); if(loading_) return;
        if(name_->text().isEmpty()) name_->setText(actionKindName(ActionKind(value)));
        if(value==int(ActionKind::Group)) label_->setChecked(true);Q_EMIT edited();
    });
    root->addStretch(); setSlot({});
}
void SlotEditor::setAdvancedSettingsVisible(bool visible) {
    styleEditor_->setVisible(visible);
}
void SlotEditor::setStore(ConfigStore* store) {
    store_=store;styleEditor_->setStore(store);libraryButton_->setEnabled(store_!=nullptr);
    if(store_)connect(store_,&ConfigStore::changed,this,[this]{
        if(iconSource_->currentIndex()!=int(IconSource::Library))return;
        const auto& assets=store_->current().assets;
        if(std::none_of(assets.begin(),assets.end(),[this](const auto& a){return a.id==libraryId_;})) {libraryId_.clear();iconSource_->setCurrentIndex(int(IconSource::Automatic));}
    });
}
void SlotEditor::setGroupsAllowed(bool allowed) {
    const QSignalBlocker blocker(kind_);
    if(allowed && kind_->count()==int(ActionKind::Group)) kind_->addItem(actionKindName(ActionKind::Group));
    if(!allowed && kind_->count()>int(ActionKind::Group)) kind_->removeItem(int(ActionKind::Group));
}
void SlotEditor::setSlot(const Slot& s) {
    styleEditor_->setStyle(s.style);libraryId_=s.icon.source==IconSource::Library?s.icon.value:QString{};
    group_=std::holds_alternative<GroupAction>(s.action)?std::get<GroupAction>(s.action):GroupAction{};
    iconTimer_->stop(); if(websiteIcon_) websiteIcon_->cancel();
    automaticUrl_=s.icon.source==IconSource::Automatic?s.icon.value:QString{};
    automaticImage_=s.icon.source==IconSource::Automatic?s.icon.image:QByteArray{};
    iconStatus_->clear(); fetchIcon_->setVisible(s.kind()==ActionKind::Website);
    loading_=true; name_->setText(s.name); kind_->setCurrentIndex(int(s.kind())); pages_->setCurrentIndex(int(s.kind()));
    shortcut_->setShortcut({}); appPath_->clear(); arguments_->clear(); directory_->clear(); normal_->setChecked(true);
    url_->clear(); browser_->setCurrentIndex(0); browserPath_->clear(); browserPath_->setEnabled(false);
    folder_->setCurrentIndex(1); folderPath_->clear(); folderPath_->setEnabled(false);
    shell_->setCurrentIndex(1); script_->clear(); commandDirectory_->clear(); hidden_->setChecked(true);
    provider_->setCurrentIndex(0); endpoint_->clear(); apiKey_->clear(); model_->clear(); resultPath_->setText("text");
    endpoint_->setEnabled(false);apiKey_->setEnabled(false);model_->setEnabled(false);resultPath_->setEnabled(false);
    window_->setCurrentIndex(int(WindowOperation::Topmost)); opacity_->setValue(85); opacity_->setEnabled(false); system_->setCurrentIndex(int(SystemOperation::Mute));
    std::visit([&](const auto& a){using T=std::decay_t<decltype(a)>;
        if constexpr(std::is_same_v<T,Shortcut>) shortcut_->setShortcut(a);
        else if constexpr(std::is_same_v<T,ApplicationAction>) {appPath_->setText(a.path); arguments_->setText(a.arguments);directory_->setText(a.directory);normal_->setChecked(a.normalUser);}
        else if constexpr(std::is_same_v<T,WebsiteAction>) {url_->setText(a.url);browser_->setCurrentIndex(int(a.browser));browserPath_->setText(a.executable);}
        else if constexpr(std::is_same_v<T,FolderAction>) {folder_->setCurrentIndex(int(a.location));folderPath_->setText(a.path);}
        else if constexpr(std::is_same_v<T,CommandAction>) {shell_->setCurrentIndex(int(a.shell));script_->setPlainText(a.script);commandDirectory_->setText(a.directory);hidden_->setChecked(a.hidden);}
        else if constexpr(std::is_same_v<T,OcrAction>) {provider_->setCurrentIndex(int(a.provider));endpoint_->setText(a.endpoint);apiKey_->setText(a.apiKey);model_->setText(a.model);resultPath_->setText(a.resultPath);}
        else if constexpr(std::is_same_v<T,WindowAction>) {window_->setCurrentIndex(int(a.operation));opacity_->setValue(a.opacity);}
        else if constexpr(std::is_same_v<T,SystemAction>) system_->setCurrentIndex(int(a.operation));
    },s.action);
    const auto source=!s.enabled() && s.icon.source==IconSource::Builtin && s.icon.value=="keyboard"?IconSource::Automatic:s.icon.source;
    iconSource_->setCurrentIndex(int(source)); icons_->setCurrentIndex(int(source)); symbol_->setCurrentIndex(symbol_->findData(s.icon.source==IconSource::Builtin?s.icon.value:"keyboard"));
    iconProgram_->setText(s.icon.source==IconSource::Program?s.icon.value:QString{}); image_=s.icon.image; label_->setChecked(s.showLabel); loading_=false;
}
Slot SlotEditor::slot() const {
    Slot s;s.style=styleEditor_->style();s.name=name_->text().trimmed();
    switch(ActionKind(kind_->currentIndex())) {
    case ActionKind::Shortcut: s.action=shortcut_->shortcut(); break;
    case ActionKind::Screenshot: s.action=ScreenshotAction{}; break;
    case ActionKind::ScreenAnnotation: s.action=AnnotationAction{}; break;
    case ActionKind::Application: s.action=ApplicationAction{appPath_->text().trimmed(),arguments_->text(),directory_->text().trimmed(),normal_->isChecked()}; break;
    case ActionKind::Website: {
        auto url=url_->text().trimmed(); if(!url.isEmpty() && !url.contains("://")) url="https://"+url;
        s.action=WebsiteAction{url,Browser(browser_->currentIndex()),browserPath_->text().trimmed()}; break;
    }
    case ActionKind::Folder: s.action=FolderAction{FolderLocation(folder_->currentIndex()),folderPath_->text().trimmed()}; break;
    case ActionKind::Command: s.action=CommandAction{Shell(shell_->currentIndex()),script_->toPlainText(),commandDirectory_->text().trimmed(),hidden_->isChecked()}; break;
    case ActionKind::Ocr: s.action=OcrAction{OcrProvider(provider_->currentIndex()),endpoint_->text().trimmed(),apiKey_->text(),model_->text().trimmed(),resultPath_->text().trimmed()}; break;
    case ActionKind::Window: s.action=WindowAction{WindowOperation(window_->currentIndex()),opacity_->value()}; break;
    case ActionKind::Group: s.action=group_;break;
    case ActionKind::System: s.action=SystemAction{SystemOperation(system_->currentIndex())}; break;
    }
    s.icon.source=IconSource(iconSource_->currentIndex()); s.icon.value=s.icon.source==IconSource::Builtin?symbol_->currentData().toString():s.icon.source==IconSource::Program?iconProgram_->text().trimmed():QString{};
    if(s.icon.source==IconSource::Library)s.icon.value=libraryId_;
    if(s.icon.source==IconSource::Image) s.icon.image=image_;
    if(s.icon.source==IconSource::Automatic && s.kind()==ActionKind::Website) {s.icon.value=automaticUrl_; s.icon.image=automaticImage_;}
    s.showLabel=label_->isChecked(); return s;
}
void SlotEditor::refreshAutomaticIcon() {
    if(loading_ || iconSource_->currentIndex()!=int(IconSource::Automatic) || kind_->currentIndex()!=int(ActionKind::Website)) return;
    const auto url=std::get<WebsiteAction>(slot().action).url;
    if(!validate(Action{WebsiteAction{url}}).isEmpty() || (url==automaticUrl_ && !automaticImage_.isEmpty())) return;
    if(!websiteIcon_) {
        websiteIcon_=new WebsiteIcon(this);
        connect(websiteIcon_,&WebsiteIcon::ready,this,[this](const QUrl& requested,const QByteArray& png,const QString& error){
            if(iconSource_->currentIndex()!=int(IconSource::Automatic) || kind_->currentIndex()!=int(ActionKind::Website) ||
               QUrl(std::get<WebsiteAction>(slot().action).url)!=requested) return;
            iconStatus_->setText(error.isEmpty()?QCoreApplication::translate("MouseWheel","网站图标已缓存"):error);
            if(!png.isEmpty()) {automaticUrl_=std::get<WebsiteAction>(slot().action).url; automaticImage_=png; Q_EMIT edited();}
        });
    }
    iconStatus_->setText(QCoreApplication::translate("MouseWheel","正在获取网站图标…")); websiteIcon_->load(QUrl(url));
}

}
