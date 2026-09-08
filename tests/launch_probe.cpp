#include <QCoreApplication>
#include <QFile>
int main(int argc,char** argv) {
    QCoreApplication app(argc,argv);
    QFile marker("launched.txt");
    return marker.open(QIODevice::WriteOnly) && marker.write("started")==7 ? 0 : 1;
}
