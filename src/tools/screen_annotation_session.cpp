#include <QCoreApplication>
#include "tools/screen_annotation_session.h"
#include "ui/theme.h"
#include <QApplication>
#include <QScreen>
#include <QToolBar>
#include <QActionGroup>
#include <QComboBox>
#include <QMouseEvent>
#include <QInputDialog>
#include <QSignalBlocker>
#include <Windows.h>
#include <utility>
namespace wheel {
class ScreenOverlay final : public QWidget {
    Q_OBJECT
public:
    ScreenOverlay(QScreen* screen,std::shared_ptr<AnnotationDocument> document,std::shared_ptr<Annotation> style)
        : document_(std::move(document)),style_(std::move(style)) {
        setObjectName("screen-annotation-overlay");
        setWindowFlags(Qt::Tool|Qt::FramelessWindowHint|Qt::WindowStaysOnTopHint|Qt::WindowDoesNotAcceptFocus);
        setAttribute(Qt::WA_ShowWithoutActivating);
        setAttribute(Qt::WA_TranslucentBackground); setScreen(screen); setGeometry(screen->geometry());
        setCursor(Qt::CrossCursor);
        connect(document_.get(),&AnnotationDocument::changed,this,[this]{dirty_=true; update();});
    }
    void setDrawing(bool drawing) {
        pending_.reset(); drawing_=drawing;
        const auto hwnd=reinterpret_cast<HWND>(winId());
        auto flags=GetWindowLongPtr(hwnd,GWL_EXSTYLE);
        flags=drawing ? flags&~WS_EX_TRANSPARENT : flags|WS_EX_TRANSPARENT;
        flags|=WS_EX_NOACTIVATE;
        SetWindowLongPtr(hwnd,GWL_EXSTYLE,flags);
        SetWindowPos(hwnd,nullptr,0,0,0,0,SWP_NOMOVE|SWP_NOSIZE|SWP_NOZORDER|SWP_NOACTIVATE|SWP_FRAMECHANGED);
        update();
    }
Q_SIGNALS:
    void textRequested(wheel::Annotation annotation);
protected:
    void paintEvent(QPaintEvent*) override {
        const auto scale=devicePixelRatioF();
        const QSize pixels(qCeil(width()*scale),qCeil(height()*scale));
        if(dirty_ || layer_.size()!=pixels || layer_.devicePixelRatio()!=scale) {
            layer_=QImage(pixels,QImage::Format_ARGB32_Premultiplied); layer_.setDevicePixelRatio(scale); layer_.fill(Qt::transparent);
            QPainter painter(&layer_); painter.translate(-geometry().topLeft()); document_->paint(painter); dirty_=false;
        }
        QImage preview=layer_;
        if(pending_) { QPainter p(&preview); p.translate(-geometry().topLeft()); AnnotationDocument::paint(p,*pending_); }
        QPainter p(this);
        // Nonzero alpha makes the entire layered window hit-testable while drawing.
        p.fillRect(rect(),QColor(0,0,0,drawing_?1:0)); p.drawImage(QPoint(0,0),preview);
    }
    void mousePressEvent(QMouseEvent* e) override {
        if(!drawing_ || e->button()!=Qt::LeftButton) return;
        pending_=*style_; pending_->points={e->globalPosition()}; update();
    }
    void mouseMoveEvent(QMouseEvent* e) override {
        if(!pending_ || pending_->tool==AnnotationTool::Text) return;
        const auto tool=pending_->tool;
        if(tool==AnnotationTool::Pen || tool==AnnotationTool::Highlighter || tool==AnnotationTool::Eraser || pending_->points.size()==1)
            pending_->points.append(e->globalPosition());
        else pending_->points.last()=e->globalPosition();
        update();
    }
    void mouseReleaseEvent(QMouseEvent* e) override {
        if(e->button()!=Qt::LeftButton || !pending_) return;
        mouseMoveEvent(e); auto annotation=std::move(*pending_); pending_.reset();
        if(annotation.tool==AnnotationTool::Text) Q_EMIT textRequested(std::move(annotation));
        else document_->add(std::move(annotation));
        update();
    }
private:
    std::shared_ptr<AnnotationDocument> document_;
    std::shared_ptr<Annotation> style_;
    std::optional<Annotation> pending_;
    QImage layer_;
    bool dirty_=true,drawing_=true;
};
ScreenAnnotationSession::ScreenAnnotationSession(QObject* parent):QObject(parent) {}
ScreenAnnotationSession::~ScreenAnnotationSession() {
    cancelTextInput();
    for(auto* overlay:overlays_) delete overlay;
    if(toolbar_) { toolbar_->removeEventFilter(this); delete toolbar_.data(); }
}
void ScreenAnnotationSession::start(Theme theme) {
    if(active()) { setDrawing(true); return; }
    document_=std::make_shared<AnnotationDocument>(); style_=std::make_shared<Annotation>();
    for(auto* screen:QApplication::screens()) {
        auto* overlay=new ScreenOverlay(screen,document_,style_); overlays_.append(overlay);
        overlay->installEventFilter(this);
        connect(overlay,&ScreenOverlay::textRequested,this,[this](Annotation annotation){
            if(textInput_ || !drawing()) return;
            auto* dialog=new QInputDialog(toolbar_); textInput_=dialog;
            dialog->setWindowTitle(QCoreApplication::translate("MouseWheel","文字标注")); dialog->setLabelText(QCoreApplication::translate("MouseWheel","文字"));
            dialog->setOkButtonText(QCoreApplication::translate("MouseWheel","确定")); dialog->setCancelButtonText(QCoreApplication::translate("MouseWheel","取消"));
            dialog->setWindowFlag(Qt::WindowStaysOnTopHint);
            connect(dialog,&QDialog::finished,this,[this,dialog,annotation=std::move(annotation)](int result) mutable {
                textInput_=nullptr;
                if(result==QDialog::Accepted && drawing()) {
                    annotation.text=dialog->textValue(); document_->add(std::move(annotation));
                }
                dialog->deleteLater();
                if(drawing()) setDrawing(true);
            });
            dialog->open();
        });
        connect(screen,&QScreen::geometryChanged,overlay,[this]{stop();});
        connect(screen,&QObject::destroyed,overlay,[this]{stop();});
        overlay->show();
    }
    toolbar_=new QToolBar; toolbar_->setObjectName("screen-annotation-toolbar");
    toolbar_->setWindowTitle(QCoreApplication::translate("MouseWheel","屏幕标注"));
    toolbar_->setWindowFlags(Qt::Tool|Qt::WindowStaysOnTopHint|Qt::WindowTitleHint|Qt::WindowCloseButtonHint|Qt::CustomizeWindowHint);
    toolbar_->setStyleSheet(settingsStyle(theme)); toolbar_->installEventFilter(this);
    auto* group=new QActionGroup(toolbar_); group->setExclusive(true);
    for(auto [label,tool]:{std::pair{"画笔",AnnotationTool::Pen},{"荧光笔",AnnotationTool::Highlighter},
            {"箭头",AnnotationTool::Arrow},{"矩形",AnnotationTool::Rectangle},{"文字",AnnotationTool::Text},{"橡皮",AnnotationTool::Eraser}}) {
        auto* action=toolbar_->addAction(QString::fromUtf8(label)); action->setCheckable(true); group->addAction(action);
        action->setChecked(tool==AnnotationTool::Pen);
        connect(action,&QAction::triggered,toolbar_,[style=style_,tool]{style->tool=tool;});
    }
    auto* colors=new QComboBox;
    colors->addItems({QCoreApplication::translate("MouseWheel","红"),QCoreApplication::translate("MouseWheel","黄"),QCoreApplication::translate("MouseWheel","蓝"),QCoreApplication::translate("MouseWheel","白"),QCoreApplication::translate("MouseWheel","黑")});
    toolbar_->addWidget(colors);
    connect(colors,&QComboBox::currentIndexChanged,toolbar_,[style=style_](int index){
        const QList<QColor> palette={Qt::red,Qt::yellow,Qt::blue,Qt::white,Qt::black}; style->color=palette[index];
    });
    auto* widths=new QComboBox; widths->addItems({QCoreApplication::translate("MouseWheel","细"),QCoreApplication::translate("MouseWheel","中"),QCoreApplication::translate("MouseWheel","粗")}); widths->setCurrentIndex(1);
    toolbar_->addWidget(widths);
    connect(widths,&QComboBox::currentIndexChanged,toolbar_,[style=style_](int index){style->width=2<<index;});
    toolbar_->addSeparator();
    toolbar_->addAction(document_->history().createUndoAction(toolbar_,QCoreApplication::translate("MouseWheel","撤销")));
    toolbar_->addAction(document_->history().createRedoAction(toolbar_,QCoreApplication::translate("MouseWheel","重做")));
    toolbar_->addAction(QCoreApplication::translate("MouseWheel","清空"),document_.get(),&AnnotationDocument::clear);
    auto* mode=toolbar_->addAction(QCoreApplication::translate("MouseWheel","操作桌面")); mode->setObjectName("desktop-mode"); mode->setCheckable(true);
    connect(mode,&QAction::toggled,this,[this](bool desktop){setDrawing(!desktop);});
    auto* exit=toolbar_->addAction(QCoreApplication::translate("MouseWheel","退出")); exit->setObjectName("exit-annotation");
    connect(exit,&QAction::triggered,this,&ScreenAnnotationSession::stop);
    toolbar_->adjustSize();
    auto* screen=QApplication::screenAt(QCursor::pos()); if(!screen) screen=QApplication::primaryScreen();
    toolbar_->setScreen(screen); toolbar_->move(screen->availableGeometry().topLeft()+QPoint(24,24));
    toolbar_->resize(toolbar_->sizeHint().boundedTo(screen->availableGeometry().size()));
    setDrawing(true);
}
void ScreenAnnotationSession::setDrawing(bool drawing) {
    if(!active()) return;
    if(!drawing) cancelTextInput();
    drawing_=drawing;
    for(auto* overlay:overlays_) { overlay->setDrawing(drawing); if(drawing) overlay->raise(); }
    auto* mode=toolbar_->findChild<QAction*>("desktop-mode"); const QSignalBlocker blocker(mode);
    mode->setChecked(!drawing); mode->setText(drawing?QCoreApplication::translate("MouseWheel","操作桌面"):QCoreApplication::translate("MouseWheel","继续绘制"));
    if(drawing) {
        toolbar_->showNormal(); toolbar_->raise(); toolbar_->activateWindow();
        if(textInput_) { textInput_->raise(); textInput_->activateWindow(); }
    }
    Q_EMIT stateChanged();
}
void ScreenAnnotationSession::cancelTextInput() {
    if(!textInput_) return;
    auto* dialog=textInput_.data(); textInput_=nullptr;
    disconnect(dialog,nullptr,this,nullptr); dialog->reject(); dialog->deleteLater();
}
void ScreenAnnotationSession::stop() {
    if(!active()) return;
    cancelTextInput();
    const auto overlays=std::exchange(overlays_,{});
    for(auto* overlay:overlays) { overlay->removeEventFilter(this); for(auto* screen:QApplication::screens()) disconnect(screen,nullptr,overlay,nullptr); overlay->hide(); overlay->deleteLater(); }
    auto* toolbar=toolbar_.data(); toolbar_=nullptr;
    toolbar->removeEventFilter(this); toolbar->hide(); toolbar->deleteLater();
    document_.reset(); style_.reset(); drawing_=false; Q_EMIT stateChanged();
}
bool ScreenAnnotationSession::eventFilter(QObject* object,QEvent* event) {
    if((object==toolbar_ && event->type()==QEvent::Close) ||
       (event->type()==QEvent::KeyPress && static_cast<QKeyEvent*>(event)->key()==Qt::Key_Escape)) {
        stop(); return true;
    }
    return QObject::eventFilter(object,event);
}
}

#include "screen_annotation_session.moc"
