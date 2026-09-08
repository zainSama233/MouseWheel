#include <QCoreApplication>
#include <QFile>
#include <QJsonDocument>
#include <QJsonArray>
int main(int argc,char** argv) {
    QCoreApplication app(argc,argv);
    QFile marker("launched.txt");
    const auto data=QJsonDocument(QJsonArray::fromStringList(app.arguments().mid(1))).toJson();
    return marker.open(QIODevice::WriteOnly) && marker.write(data)==data.size()?0:1;
}
