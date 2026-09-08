#pragma once
#include "tools/region_capture.h"
#include "tools/local_ocr.h"
#include <QPointer>
#include <QFutureWatcher>
class QDialog; class QPlainTextEdit; class QNetworkAccessManager; class QNetworkReply;
namespace wheel {
class OcrSession final:public QObject {
    Q_OBJECT
public:
    explicit OcrSession(QObject* parent=nullptr);
    ~OcrSession() override;
    bool active() const;
    bool start(OcrAction action,Theme theme,QString& error);
    void cancel();
    void recognize(QImage image,OcrAction action,Theme theme);
Q_SIGNALS:
    void activeChanged();
    void completed(QString text,QString error);
private:
    void finish(quint64 generation,OcrResult result);
    RegionCapture capture_;
    QPointer<QDialog> result_;
    QPointer<QPlainTextEdit> text_;
    QPointer<QNetworkReply> reply_;
    QNetworkAccessManager* network_=nullptr;
    quint64 generation_=0;
    OcrAction action_;
    Theme theme_=Theme::Light;
};
}
