#include "ui/localization.h"
#include <QApplication>
#include <QFile>
#include <QJsonDocument>
#include <QJsonArray>
#include <QEvent>
#include <QLabel>
#include <QAbstractButton>
#include <QLineEdit>
#include <QComboBox>
#include <QSignalBlocker>
int qInitResources_locales();
namespace wheel {
Localization& Localization::instance(){static auto* instance=new Localization;return *instance;}
Localization::Localization():QTranslator(qApp) {
    ::qInitResources_locales();
    QFile file(":/locales/catalog.json");file.open(QIODevice::ReadOnly);catalog_=QJsonDocument::fromJson(file.readAll()).object();
    for(auto it=catalog_.begin();it!=catalog_.end();++it){sources_.insert(it.key(),it.key());for(const auto& value:it.value().toArray())sources_.insert(value.toString(),it.key());}
    qApp->installEventFilter(this);qApp->installTranslator(this);
}
QString Localization::translate(const char*,const char* source,const char*,int) const {
    const int language=language_.load();if(!language)return QString::fromUtf8(source);
    const auto values=catalog_.value(QString::fromUtf8(source)).toArray();return values.size()==3?values[language-1].toString():QString{};
}
void Localization::setLanguage(Language language) {
    if(language_.exchange(int(language))==int(language))return;
    for(auto* widget:QApplication::allWidgets()){QEvent event(QEvent::LanguageChange);QCoreApplication::sendEvent(widget,&event);}
}
bool Localization::eventFilter(QObject* object,QEvent* event) {
    if(event->type()!=QEvent::Polish && event->type()!=QEvent::LanguageChange)return false;
    auto* widget=qobject_cast<QWidget*>(object);if(!widget || widget->property("locale-user-content").toBool())return false;
    const QSignalBlocker blocker(widget);
    const auto update=[&](const char* property){
        const auto value=widget->property(property).toString();
        const QString source=sources_.value(value);if(source.isEmpty())return;
        widget->setProperty(property,translate("MouseWheel",source.toUtf8()));
    };
    update("windowTitle");update("toolTip");update("specialValueText");
    if(qobject_cast<QLabel*>(widget) || qobject_cast<QAbstractButton*>(widget))update("text");
    if(qobject_cast<QLineEdit*>(widget))update("placeholderText");
    if(auto* combo=qobject_cast<QComboBox*>(widget);combo && !combo->property("locale-user-items").toBool()) {
        combo->setSizeAdjustPolicy(QComboBox::AdjustToContents);
        for(int i=0;i<combo->count();++i){const auto source=sources_.value(combo->itemText(i));if(!source.isEmpty())combo->setItemText(i,translate("MouseWheel",source.toUtf8()));}
    }
    widget->updateGeometry();
    return false;
}
}
