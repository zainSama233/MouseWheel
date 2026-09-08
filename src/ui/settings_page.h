#pragma once
#include <QScrollArea>
#include <QVBoxLayout>
#include <QLabel>
namespace wheel {
class SettingsPage final : public QScrollArea {
public:
    explicit SettingsPage(QWidget* parent=nullptr):QScrollArea(parent) {
        setWidgetResizable(true);setFrameShape(QFrame::NoFrame);
        auto* content=new QWidget;body_=new QVBoxLayout(content);
        body_->setContentsMargins(4,4,16,16);body_->setSpacing(16);body_->setAlignment(Qt::AlignTop);
        setWidget(content);
    }
    QVBoxLayout* body() const {return body_;}
    void addSection(const QString& title) {
        auto* label=new QLabel(title);label->setObjectName("section");body_->addWidget(label);
    }
private:
    QVBoxLayout* body_;
};
}
