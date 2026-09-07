#include "tools/image_editor.h"
#include "ui/theme.h"
#include <QVBoxLayout>
#include <QToolBar>
#include <QActionGroup>
#include <QMouseEvent>
#include <QInputDialog>
#include <QFileDialog>
#include <QMessageBox>
#include <QApplication>
#include <QClipboard>
#include <QScreen>
#include <QComboBox>
#include <QLabel>
#include <algorithm>
namespace wheel {
AnnotationCanvas::AnnotationCanvas(AnnotationDocument& document,QWidget* parent) : QWidget(parent),document_(document) {
    setObjectName("annotation-canvas"); setMinimumSize(200,150); setCursor(Qt::CrossCursor);
    connect(&document_,&AnnotationDocument::changed,this,qOverload<>(&QWidget::update));
}
QRectF AnnotationCanvas::imageRect() const {
    QSizeF size=document_.image().size(); size.scale(this->size(),Qt::KeepAspectRatio);
    return {(width()-size.width())/2,(height()-size.height())/2,size.width(),size.height()};
}
QPointF AnnotationCanvas::imagePoint(QPointF point) const {
    const auto rect=imageRect();
    return {std::clamp((point.x()-rect.x())*document_.image().width()/rect.width(),0.0,double(document_.image().width()-1)),
            std::clamp((point.y()-rect.y())*document_.image().height()/rect.height(),0.0,double(document_.image().height()-1))};
}
void AnnotationCanvas::paintEvent(QPaintEvent*) {
    QPainter p(this); p.fillRect(rect(),QColor("#30343b"));
    const auto area=imageRect(); p.drawImage(area,document_.image());
    if(pending_) {
        p.setClipRect(area); p.translate(area.topLeft());
        p.scale(area.width()/document_.image().width(),area.height()/document_.image().height());
        AnnotationDocument::paint(p,*pending_,document_.image());
    }
}
void AnnotationCanvas::mousePressEvent(QMouseEvent* event) {
    if(event->button()!=Qt::LeftButton || !imageRect().contains(event->position())) return;
    const auto point=imagePoint(event->position());
    Annotation annotation{tool_,{point},color_,width_,{}};
    if(tool_==AnnotationTool::Text) {
        bool ok=false;
        annotation.text=QInputDialog::getText(this,QStringLiteral("文字标注"),QStringLiteral("文字"),QLineEdit::Normal,{},&ok);
        if(ok) document_.add(std::move(annotation));
    } else { pending_=std::move(annotation); update(); }
}
void AnnotationCanvas::mouseMoveEvent(QMouseEvent* event) {
    if(!pending_) return;
    const auto point=imagePoint(event->position());
    if(pending_->tool==AnnotationTool::Pen || pending_->points.size()==1) pending_->points.append(point);
    else pending_->points.last()=point;
    update();
}
void AnnotationCanvas::mouseReleaseEvent(QMouseEvent* event) {
    if(event->button()!=Qt::LeftButton || !pending_) return;
    mouseMoveEvent(event); document_.add(std::move(*pending_)); pending_.reset(); update();
}
ImageEditor::ImageEditor(QImage image,Theme theme) : document_(std::move(image)) {
    setAttribute(Qt::WA_DeleteOnClose); setWindowTitle(QStringLiteral("截图标注"));
    setStyleSheet(settingsStyle(theme));
    auto* root=new QVBoxLayout(this);
    auto* tools=new QToolBar; tools->setIconSize({16,16}); root->addWidget(tools);
    auto* canvas=new AnnotationCanvas(document_,this);
    auto* group=new QActionGroup(this); group->setExclusive(true);
    for(auto [label,tool] : {std::pair{"画笔",AnnotationTool::Pen},{"矩形",AnnotationTool::Rectangle},
        {"箭头",AnnotationTool::Arrow},{"文字",AnnotationTool::Text},{"马赛克",AnnotationTool::Mosaic}}) {
        auto* action=tools->addAction(QString::fromUtf8(label)); action->setCheckable(true);
        action->setObjectName(QString("tool-%1").arg(static_cast<int>(tool))); group->addAction(action);
        action->setChecked(tool==AnnotationTool::Pen);
        connect(action,&QAction::triggered,this,[canvas,tool]{canvas->setTool(tool);});
    }
    tools->addSeparator();
    auto* colors=new QComboBox; colors->addItems({QStringLiteral("红"),QStringLiteral("黄"),QStringLiteral("蓝"),QStringLiteral("白"),QStringLiteral("黑")});
    const QList<QColor> palette={QColor("#ef4444"),QColor("#facc15"),QColor("#3b82f6"),Qt::white,Qt::black};
    tools->addWidget(colors); connect(colors,&QComboBox::currentIndexChanged,this,[canvas,palette](int index){canvas->setColor(palette[index]);});
    auto* widths=new QComboBox; widths->addItems({QStringLiteral("细"),QStringLiteral("中"),QStringLiteral("粗")}); widths->setCurrentIndex(1);
    tools->addWidget(widths); connect(widths,&QComboBox::currentIndexChanged,this,[canvas](int index){canvas->setWidth(2<<index);});
    tools->addSeparator();
    auto* undo=document_.history().createUndoAction(this,QStringLiteral("撤销")); undo->setShortcut(QKeySequence::Undo); undo->setObjectName("undo"); tools->addAction(undo);
    auto* redo=document_.history().createRedoAction(this,QStringLiteral("重做")); redo->setShortcut(QKeySequence::Redo); tools->addAction(redo);
    root->addWidget(canvas,1);
    auto* output=new QToolBar; root->addWidget(output);
    output->addWidget(new QLabel(QStringLiteral("%1 × %2 像素").arg(document_.image().width()).arg(document_.image().height())));
    output->addSeparator();
    auto* copy=output->addAction(QStringLiteral("复制")); copy->setObjectName("copy-image");
    connect(copy,&QAction::triggered,this,[this]{QApplication::clipboard()->setImage(document_.image()); close();});
    auto* save=output->addAction(QStringLiteral("保存 PNG"));
    connect(save,&QAction::triggered,this,[this]{
        const auto path=QFileDialog::getSaveFileName(this,QStringLiteral("保存截图"),QStringLiteral("截图.png"),QStringLiteral("PNG 图片 (*.png)"));
        if(path.isEmpty()) return;
        QString error; if(document_.savePng(path,error)) close();
        else QMessageBox::warning(this,QStringLiteral("保存失败"),error);
    });
    output->addAction(QStringLiteral("取消"),this,&QWidget::close);
    resize((screen()->availableGeometry().size()-QSize(48,64)).boundedTo({1000,720}));
}
void ImageEditor::keyPressEvent(QKeyEvent* event) {
    if(event->key()==Qt::Key_Escape) close(); else QWidget::keyPressEvent(event);
}
}
