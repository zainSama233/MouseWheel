#include "tools/ocr_session.h"
#include "ui/theme.h"
#include <QtConcurrent>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QBuffer>
#include <QDialog>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QVBoxLayout>
#include <QApplication>
#include <QClipboard>
namespace wheel {
OcrSession::OcrSession(QObject* parent):QObject(parent) {
    connect(&capture_,&RegionCapture::activeChanged,this,&OcrSession::activeChanged);
    connect(&capture_,&RegionCapture::selected,this,[this](QImage image,QScreen*){recognize(image,action_,theme_);});
}
OcrSession::~OcrSession() {disconnect(&capture_,nullptr,this,nullptr);cancel();}
bool OcrSession::active() const {return capture_.active();}
void OcrSession::cancel() {
    ++generation_; capture_.cancel();
    if(reply_) {reply_->abort(); reply_->deleteLater();reply_=nullptr;}
    if(result_) {disconnect(result_,nullptr,this,nullptr);delete result_.data();} text_=nullptr;
}
bool OcrSession::start(OcrAction action,Theme theme,QString& error) {
    cancel(); action_=std::move(action);theme_=theme;return capture_.start(theme,error);
}
void OcrSession::recognize(QImage image,OcrAction action,Theme theme) {
    cancel(); const auto generation=generation_;
    auto* dialog=new QDialog; result_=dialog; dialog->setAttribute(Qt::WA_DeleteOnClose);
    dialog->setWindowTitle(QStringLiteral("屏幕 OCR"));dialog->resize(540,360);dialog->setStyleSheet(settingsStyle(theme));
    auto* layout=new QVBoxLayout(dialog);auto* text=new QPlainTextEdit; text_=text;text->setReadOnly(true);text->setPlaceholderText(QStringLiteral("正在识别…"));layout->addWidget(text);
    auto* copy=new QPushButton(QStringLiteral("复制文字"));copy->setEnabled(false);layout->addWidget(copy);
    connect(copy,&QPushButton::clicked,dialog,[text]{QApplication::clipboard()->setText(text->toPlainText());});
    connect(text,&QPlainTextEdit::textChanged,copy,[text,copy]{copy->setEnabled(!text->toPlainText().isEmpty());});
    connect(dialog,&QDialog::finished,this,[this,generation]{if(generation==generation_) {++generation_;if(reply_) reply_->abort();}});
    dialog->show();dialog->raise();dialog->activateWindow();
    const auto invalid=validate(Action{action});
    if(!invalid.isEmpty() || image.isNull()) {finish(generation,{{},invalid.isEmpty()?QStringLiteral("识别图片为空。"):invalid});return;}
    if(action.provider==OcrProvider::Local) {
        auto* watcher=new QFutureWatcher<OcrResult>(this);
        connect(watcher,&QFutureWatcher<OcrResult>::finished,this,[this,watcher,generation]{const auto result=watcher->result();watcher->deleteLater();finish(generation,result);});
        watcher->setFuture(QtConcurrent::run([image]{return recognizeLocal(image);}));return;
    }
    if(!network_) network_=new QNetworkAccessManager(this);
    QByteArray png;QBuffer buffer(&png);buffer.open(QIODevice::WriteOnly);image.save(&buffer,"PNG");
    if(png.size()>20*1024*1024) {finish(generation,{{},QStringLiteral("识别区域过大，请缩小框选范围。")});return;}
    QJsonObject body;
    if(action.provider==OcrProvider::Ai) {
        const QJsonArray content{QJsonObject{{"type","text"},{"text","Extract all text from this image. Preserve line breaks. Return only the recognized text."}},QJsonObject{{"type","image_url"},{"image_url",QJsonObject{{"url","data:image/png;base64,"+QString::fromLatin1(png.toBase64())}}}}};
        body={{"model",action.model},{"messages",QJsonArray{QJsonObject{{"role","user"},{"content",content}}}}};
    } else body={{"image",QString::fromLatin1(png.toBase64())},{"mimeType","image/png"}};
    QNetworkRequest request{QUrl(action.endpoint)};request.setHeader(QNetworkRequest::ContentTypeHeader,"application/json");request.setTransferTimeout(30000);
    request.setAttribute(QNetworkRequest::RedirectPolicyAttribute,QNetworkRequest::ManualRedirectPolicy);
    if(!action.apiKey.isEmpty()) request.setRawHeader("Authorization","Bearer "+action.apiKey.toUtf8());
    auto* reply=network_->post(request,QJsonDocument(body).toJson(QJsonDocument::Compact));reply_=reply;
    auto bytes=std::make_shared<QByteArray>();
    connect(reply,&QNetworkReply::readyRead,this,[reply,bytes]{bytes->append(reply->readAll());if(bytes->size()>4*1024*1024) reply->abort();});
    connect(reply,&QNetworkReply::finished,this,[this,reply,bytes,generation,action]{
        reply->deleteLater(); if(generation!=generation_) return; bytes->append(reply->readAll());
        const int status=reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        if(reply->error()!=QNetworkReply::NoError || status<200 || status>=300 || bytes->size()>4*1024*1024) {finish(generation,{{},QStringLiteral("识别请求失败（HTTP %1）：%2").arg(status).arg(reply->errorString())});return;}
        QJsonParseError parse;const auto doc=QJsonDocument::fromJson(*bytes,&parse);
        QJsonValue value=doc.isArray()?QJsonValue(doc.array()):QJsonValue(doc.object());
        const auto path=action.provider==OcrProvider::Ai?QString("choices.0.message.content"):action.resultPath;
        for(const auto& part:path.split('.')) {
            if(value.isArray()) {bool ok=false;const int index=part.toInt(&ok);value=ok && index>=0 && index<value.toArray().size()?value.toArray().at(index):QJsonValue{};}
            else value=value.toObject().value(part);
        }
        if(parse.error!=QJsonParseError::NoError || !value.isString()) {finish(generation,{{},QStringLiteral("响应不包含可读取的文字字段。")});return;}
        finish(generation,{value.toString(),{}});
    });
}
void OcrSession::finish(quint64 generation,OcrResult result) {
    if(generation!=generation_ || !text_) return;
    if(!result.error.isEmpty()) text_->setPlaceholderText(result.error);
    else {text_->setReadOnly(false);text_->setPlaceholderText(QStringLiteral("未识别到文字"));text_->setPlainText(result.text);}
    Q_EMIT completed(result.text,result.error);
}
}
