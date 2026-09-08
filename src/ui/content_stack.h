#pragma once
#include <QStackedWidget>
namespace wheel {
class ContentStack final : public QStackedWidget {
public:
    explicit ContentStack(QWidget* parent=nullptr):QStackedWidget(parent) {
        setSizePolicy(QSizePolicy::Preferred,QSizePolicy::Maximum);
        connect(this,&QStackedWidget::currentChanged,this,[this]{updateGeometry();});
    }
    QSize sizeHint() const override {return currentWidget()?currentWidget()->sizeHint():QSize{};}
    QSize minimumSizeHint() const override {return currentWidget()?currentWidget()->minimumSizeHint():QSize{};}
};
}
