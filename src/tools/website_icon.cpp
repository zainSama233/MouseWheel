#include <QCoreApplication>
#include "tools/website_icon.h"
#include "core/image_asset.h"
#include <QNetworkReply>
#include <QRegularExpression>
#include <QTextDocumentFragment>
namespace wheel {
WebsiteIcon::~WebsiteIcon() { cancel(); }
void WebsiteIcon::cancel() {
    if(reply_) {disconnect(reply_,nullptr,this,nullptr); reply_->abort(); reply_->deleteLater(); reply_=nullptr;}
    candidates_.clear();
}
void WebsiteIcon::load(const QUrl& page) {
    cancel(); page_=page;
    if(!page.isValid() || page.host().isEmpty() || (page.scheme()!="https" && page.scheme()!="http")) {
        Q_EMIT ready(page_,{},QCoreApplication::translate("MouseWheel","网址无效")); return;
    }
    request(page,true);
}
void WebsiteIcon::request(const QUrl& url,bool page) {
    QNetworkRequest networkRequest(url); networkRequest.setTransferTimeout(8000);
    networkRequest.setMaximumRedirectsAllowed(5);
    networkRequest.setHeader(QNetworkRequest::UserAgentHeader,"MouseWheel/0.5");
    auto* reply=network_.get(networkRequest); reply_=reply; reply->setReadBufferSize(1048577);
    auto bytes=std::make_shared<QByteArray>();
    connect(reply,&QNetworkReply::readyRead,this,[reply,bytes]{
        bytes->append(reply->read(1048577-bytes->size())); if(bytes->size()>1048576) reply->abort();
    });
    connect(reply,&QNetworkReply::finished,this,[this,reply,bytes,page]{
        reply_=nullptr; reply->deleteLater();
        if(page) {
            const auto html=QString::fromUtf8(*bytes);
            const QRegularExpression links(R"(<link\b([^>]*)>)",QRegularExpression::CaseInsensitiveOption);
            const QRegularExpression attributes(R"attr(([\w:-]+)\s*=\s*(?:"([^"]*)"|'([^']*)'|([^\s>]+)))attr");
            auto matches=links.globalMatch(html);
            while(matches.hasNext() && candidates_.size()<6) {
                QHash<QString,QString> values; auto fields=attributes.globalMatch(matches.next().captured(1));
                while(fields.hasNext()) {const auto field=fields.next(); values[field.captured(1).toLower()]=field.captured(2)+field.captured(3)+field.captured(4);}
                const auto rel=values.value("rel").toLower().split(QRegularExpression("\\s+"));
                if(!rel.contains("icon") && !rel.contains("apple-touch-icon")) continue;
                const auto href=QTextDocumentFragment::fromHtml(values.value("href")).toPlainText();
                const auto target=reply->url().resolved(QUrl(href));
                if(!href.isEmpty() && (target.scheme()=="https" || target.scheme()=="http") && !candidates_.contains(target)) candidates_.append(target);
            }
            const auto fallback=reply->url().resolved(QUrl("/favicon.ico"));
            if(!candidates_.contains(fallback)) candidates_.append(fallback);
        } else if(reply->error()==QNetworkReply::NoError && bytes->size()<=1048576) {
            QByteArray png; QString error;
            if(importImageAsset(*bytes,png,error)) {Q_EMIT ready(page_,png,{});return;}
        }
        if(candidates_.isEmpty()) Q_EMIT ready(page_,{},QCoreApplication::translate("MouseWheel","未找到网站图标，可重试或选择自定义图片"));
        else request(candidates_.takeFirst(),false);
    });
}
}
