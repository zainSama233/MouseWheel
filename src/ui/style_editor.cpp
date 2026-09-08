#include <QCoreApplication>
#include "ui/style_editor.h"
#include "config/config_store.h"
#include "tools/region_capture.h"
#include <QToolButton>
#include <QPushButton>
#include <QLineEdit>
#include <QDoubleSpinBox>
#include <QComboBox>
#include <QColorDialog>
#include <QInputDialog>
#include <QMessageBox>
#include <QFormLayout>
#include <QVBoxLayout>
#include <QMenu>
#include <QUuid>
namespace wheel {
StyleEditor::StyleEditor(QWidget* parent):QWidget(parent) {
    auto* root=new QVBoxLayout(this);root->setContentsMargins(0,0,0,0);
    auto* toggle=new QToolButton;toggle->setText(QCoreApplication::translate("MouseWheel","颜色与排版"));toggle->setCheckable(true);toggle->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);toggle->setArrowType(Qt::RightArrow);root->addWidget(toggle);
    auto* body=new QWidget;body->hide();root->addWidget(body);auto* form=new QFormLayout(body);
    connect(toggle,&QToolButton::toggled,body,&QWidget::setVisible);connect(toggle,&QToolButton::toggled,this,[toggle](bool on){toggle->setArrowType(on?Qt::DownArrow:Qt::RightArrow);});
    const QStringList colorNames{QCoreApplication::translate("MouseWheel","底色"),QCoreApplication::translate("MouseWheel","高亮光晕"),QCoreApplication::translate("MouseWheel","边框"),QCoreApplication::translate("MouseWheel","文字")};
    for(int i=0;i<4;++i) {
        auto* row=new QHBoxLayout;auto* input=new QLineEdit;colors_[i]=input;input->setObjectName(QString("style-color-%1").arg(i));input->setPlaceholderText(QCoreApplication::translate("MouseWheel","继承 / #RRGGBB"));row->addWidget(input);
        auto* palette=new QPushButton(QCoreApplication::translate("MouseWheel","色盘"));auto* sample=new QPushButton(QCoreApplication::translate("MouseWheel","吸色"));auto* presets=new QPushButton(QCoreApplication::translate("MouseWheel","预设"));row->addWidget(palette);row->addWidget(sample);row->addWidget(presets);form->addRow(colorNames[i],row);
        connect(input,&QLineEdit::textChanged,this,[this,input,i]{
            if(loading_)return;const bool valid=input->text().isEmpty() || QColor::isValidColorName(input->text());
            input->setToolTip(valid?QString{}:QCoreApplication::translate("MouseWheel","颜色无效。"));if(!valid)return;
            colorValues_[i]=input->text().isEmpty()?std::nullopt:std::optional<QColor>(QColor(input->text()));Q_EMIT edited();
        });
        connect(palette,&QPushButton::clicked,this,[this,input]{
            auto* dialog=new QColorDialog(QColor(input->text()),this);dialog->setAttribute(Qt::WA_DeleteOnClose);dialog->setOption(QColorDialog::ShowAlphaChannel);dialog->setOption(QColorDialog::DontUseNativeDialog);
            connect(dialog,&QColorDialog::currentColorChanged,input,[input](const QColor& color){input->setText(color.name(QColor::HexArgb));});dialog->open();
        });
        connect(sample,&QPushButton::clicked,this,[this,input]{
            auto* capture=new RegionCapture(this);connect(capture,&RegionCapture::selected,input,[input](const QImage& image,QScreen*){input->setText(image.pixelColor(0,0).name(QColor::HexArgb));});
            connect(capture,&RegionCapture::activeChanged,capture,[capture]{if(!capture->active())capture->deleteLater();});
            QString error;if(!capture->start(Theme::Dark,error,RegionCapture::Mode::Pixel)){QMessageBox::warning(this,QCoreApplication::translate("MouseWheel","吸色失败"),error);capture->deleteLater();}
        });
        connect(presets,&QPushButton::clicked,this,[this,input,presets]{
            if(!store_)return;auto* menu=new QMenu(this);menu->setAttribute(Qt::WA_DeleteOnClose);
            auto* save=menu->addAction(QCoreApplication::translate("MouseWheel","保存当前颜色…"));save->setEnabled(QColor::isValidColorName(input->text()));
            connect(save,&QAction::triggered,this,[this,input]{bool ok=false;const auto name=QInputDialog::getText(this,QCoreApplication::translate("MouseWheel","颜色预设"),QCoreApplication::translate("MouseWheel","名称"),QLineEdit::Normal,{},&ok).trimmed();if(!ok || name.isEmpty())return;
                auto c=store_->current();c.colors.append({QUuid::createUuid().toString(QUuid::WithoutBraces),name,QColor(input->text())});if(!store_->commit(c))QMessageBox::warning(this,QCoreApplication::translate("MouseWheel","保存失败"),store_->error());});
            for(const auto& preset:store_->current().colors) {
                auto* item=menu->addMenu(preset.name);item->setProperty("locale-user-content",true);connect(item->addAction(QCoreApplication::translate("MouseWheel","使用")),&QAction::triggered,input,[input,preset]{input->setText(preset.color.name(QColor::HexArgb));});
                connect(item->addAction(QCoreApplication::translate("MouseWheel","重命名")),&QAction::triggered,this,[this,preset]{bool ok=false;const auto name=QInputDialog::getText(this,QCoreApplication::translate("MouseWheel","重命名颜色"),QCoreApplication::translate("MouseWheel","名称"),QLineEdit::Normal,preset.name,&ok).trimmed();if(!ok || name.isEmpty())return;auto c=store_->current();for(auto& color:c.colors)if(color.id==preset.id)color.name=name;if(!store_->commit(c))QMessageBox::warning(this,QCoreApplication::translate("MouseWheel","保存失败"),store_->error());});
                connect(item->addAction(QCoreApplication::translate("MouseWheel","删除")),&QAction::triggered,this,[this,preset]{auto c=store_->current();c.colors.removeIf([&](const auto& color){return color.id==preset.id;});if(!store_->commit(c))QMessageBox::warning(this,QCoreApplication::translate("MouseWheel","保存失败"),store_->error());});
            }menu->popup(presets->mapToGlobal(QPoint(0,presets->height())));
        });
    }
    font_=new QLineEdit;font_->setPlaceholderText(QCoreApplication::translate("MouseWheel","继承字体"));font_->setObjectName("style-font");form->addRow(QCoreApplication::translate("MouseWheel","字体"),font_);connect(font_,&QLineEdit::textChanged,this,[this]{if(!loading_)Q_EMIT edited();});
    layout_=new QComboBox;layout_->addItems({QCoreApplication::translate("MouseWheel","继承排版"),QCoreApplication::translate("MouseWheel","文字在下"),QCoreApplication::translate("MouseWheel","文字在上"),QCoreApplication::translate("MouseWheel","文字在左"),QCoreApplication::translate("MouseWheel","文字在右"),QCoreApplication::translate("MouseWheel","仅图标")});layout_->setObjectName("style-layout");form->addRow(QCoreApplication::translate("MouseWheel","排版"),layout_);connect(layout_,&QComboBox::currentIndexChanged,this,[this]{if(!loading_)Q_EMIT edited();});
    const QStringList names{QCoreApplication::translate("MouseWheel","字号"),QCoreApplication::translate("MouseWheel","图标大小"),QCoreApplication::translate("MouseWheel","X 偏移"),QCoreApplication::translate("MouseWheel","Y 偏移"),QCoreApplication::translate("MouseWheel","线宽"),QCoreApplication::translate("MouseWheel","光晕半径")};
    const std::array<double,6> minima{8,12,-128,-128,0,0},maxima{48,96,128,128,8,24};
    for(int i=0;i<6;++i) {auto* number=new QDoubleSpinBox;numbers_[i]=number;number->setObjectName(QString("style-number-%1").arg(i));number->setDecimals(0);number->setRange(minima[i]-1,maxima[i]);number->setSpecialValueText(QCoreApplication::translate("MouseWheel","继承"));form->addRow(names[i],number);connect(number,&QDoubleSpinBox::valueChanged,this,[this]{if(!loading_)Q_EMIT edited();});}
    auto* reset=new QPushButton(QCoreApplication::translate("MouseWheel","恢复继承"));form->addRow(reset);connect(reset,&QPushButton::clicked,this,[this]{setStyle({});Q_EMIT edited();});setStyle({});
}
void StyleEditor::setStyle(const SlotStyle& s) {
    loading_=true;const std::array<std::optional<QColor>,4> colors{s.fill,s.glow,s.border,s.text};colorValues_=colors;
    for(int i=0;i<4;++i)colors_[i]->setText(colors[i]?colors[i]->name(QColor::HexArgb):QString{});
    font_->setText(s.fontFamily.value_or(QString{}));layout_->setCurrentIndex(s.layout?int(*s.layout)+1:0);
    const std::array<std::optional<double>,6> values{s.fontSize?std::optional<double>(*s.fontSize):std::nullopt,s.iconSize?std::optional<double>(*s.iconSize):std::nullopt,s.offsetX,s.offsetY,s.borderWidth,s.glowRadius};
    for(int i=0;i<6;++i)numbers_[i]->setValue(values[i].value_or(numbers_[i]->minimum()));loading_=false;
}
SlotStyle StyleEditor::style() const {
    SlotStyle s;const std::array<std::optional<QColor> SlotStyle::*,4> members{&SlotStyle::fill,&SlotStyle::glow,&SlotStyle::border,&SlotStyle::text};
    for(int i=0;i<4;++i)s.*members[i]=colorValues_[i];
    if(!font_->text().trimmed().isEmpty())s.fontFamily=font_->text().trimmed();if(layout_->currentIndex()>0)s.layout=ContentLayout(layout_->currentIndex()-1);
    if(numbers_[0]->value()>numbers_[0]->minimum())s.fontSize=int(numbers_[0]->value());if(numbers_[1]->value()>numbers_[1]->minimum())s.iconSize=int(numbers_[1]->value());
    const std::array<std::optional<double> SlotStyle::*,4> fields{&SlotStyle::offsetX,&SlotStyle::offsetY,&SlotStyle::borderWidth,&SlotStyle::glowRadius};
    for(int i=0;i<4;++i)if(numbers_[i+2]->value()>numbers_[i+2]->minimum())s.*fields[i]=numbers_[i+2]->value();return s;
}
}
