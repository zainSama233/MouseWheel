#pragma once
#include <QTranslator>
#include <QHash>
#include <QJsonObject>
#include <atomic>
#include "core/model.h"
namespace wheel {
class Localization final:public QTranslator {
public:
    static Localization& instance();
    void setLanguage(Language language);
    Language language() const {return Language(language_.load());}
    QString translate(const char*,const char* source,const char* =nullptr,int =-1) const override;
    bool isEmpty() const override {return false;}
protected:
    bool eventFilter(QObject*,QEvent*) override;
private:
    Localization();
    QJsonObject catalog_;
    QHash<QString,QString> sources_;
    std::atomic<int> language_{0};
};
}
