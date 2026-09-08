#include "ui/library_icon.h"
#include <QIconEngine>
#include <QImageReader>
#include <QPixmapCache>
#include <QPainter>
#include <QFileInfo>
namespace wheel {
class LibraryIconEngine final:public QIconEngine {
public:
    explicit LibraryIconEngine(QString path):path_(std::move(path)){}
    QIconEngine* clone() const override{return new LibraryIconEngine(path_);}
    QPixmap pixmap(const QSize& size,QIcon::Mode,QIcon::State) override {
        const auto key=QString("library:%1:%2:%3").arg(path_).arg(size.width()).arg(size.height());QPixmap result;
        if(QPixmapCache::find(key,&result))return result;
        QImageReader reader(path_);reader.setAutoTransform(true);
        if(path_.endsWith(".ico",Qt::CaseInsensitive)) {
            int best=0;int score=INT_MAX;
            for(int i=0;i<reader.imageCount();++i){if(!reader.jumpToImage(i))continue;const auto candidate=reader.size();const int difference=qAbs(candidate.width()-size.width())+(candidate.width()<size.width()?4096:0);if(difference<score){best=i;score=difference;}}
            reader.jumpToImage(best);
        }
        reader.setScaledSize(reader.size().scaled(size,Qt::KeepAspectRatio));
        const auto image=reader.read();if(image.isNull())return {};
        result=QPixmap::fromImage(image.scaled(size,Qt::KeepAspectRatio,Qt::SmoothTransformation));QPixmapCache::insert(key,result);return result;
    }
    void paint(QPainter* painter,const QRect& rect,QIcon::Mode mode,QIcon::State state) override {
        const auto size=painter->deviceTransform().mapRect(QRectF(rect)).size().toSize();const auto image=pixmap(size,mode,state);if(image.isNull())return;
        QSizeF target=image.size();target.scale(rect.size(),Qt::KeepAspectRatio);const QRectF area(QPointF(rect.center())-QPointF(target.width()/2,target.height()/2),target);painter->drawPixmap(area,image,image.rect());
    }
private:
    QString path_;
};
QIcon libraryIcon(const QString& path){if(!QFileInfo::exists(path))return {};return QIcon(new LibraryIconEngine(path));}
}
