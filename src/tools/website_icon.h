#pragma once
#include <QObject>
#include <QNetworkAccessManager>
#include <QPointer>
#include <QUrl>
#include <QList>
class QNetworkReply;
namespace wheel {
class WebsiteIcon final:public QObject {
    Q_OBJECT
public:
    explicit WebsiteIcon(QObject* parent=nullptr):QObject(parent) {}
    ~WebsiteIcon() override;
    void load(const QUrl& page);
    void cancel();
Q_SIGNALS:
    void ready(const QUrl& page,const QByteArray& png,const QString& error);
private:
    void request(const QUrl& url,bool page);
    QNetworkAccessManager network_;
    QPointer<QNetworkReply> reply_;
    QUrl page_;
    QList<QUrl> candidates_;
};
}
