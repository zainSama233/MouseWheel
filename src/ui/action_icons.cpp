#include "ui/action_icons.h"
#include <QSvgRenderer>
#include <QFile>
#include <QFileIconProvider>
#include <QPainter>
int qInitResources_icons();
namespace wheel {
QIcon symbolIcon(const QString& name,const QColor& color) {
    static const bool initialized=::qInitResources_icons()!=0; Q_UNUSED(initialized);
    QFile file(":/icons/"+name+".svg"); if(!file.open(QIODevice::ReadOnly)) return {};
    auto svg=file.readAll(); svg.replace("currentColor",color.name().toUtf8());
    QSvgRenderer renderer(svg); QPixmap image(96,96); image.fill(Qt::transparent);
    QPainter painter(&image); renderer.render(&painter); return QIcon(image);
}
QIcon actionIcon(const Slot& slot,const QColor& color) {
    QString symbol="keyboard";
    switch(slot.kind) {
    case ActionKind::Screenshot: symbol="camera"; break;
    case ActionKind::ScreenAnnotation: symbol="pencil"; break;
    case ActionKind::Website: symbol="globe"; break;
    case ActionKind::Application: {
        QFileIconProvider provider; const auto icon=provider.icon(QFileInfo(slot.target));
        if(!icon.isNull()) return icon;
        symbol="app-window"; break;
    }
    case ActionKind::Shortcut:
        if(!slot.enabled()) symbol="plus";
        else if(slot.shortcut.modifiers==bit(Modifier::Control)) {
            switch(slot.shortcut.key) {
            case Qt::Key_C: symbol="copy"; break;
            case Qt::Key_V: symbol="clipboard-paste"; break;
            case Qt::Key_X: symbol="scissors"; break;
            case Qt::Key_Z: symbol="undo-2"; break;
            case Qt::Key_Y: symbol="redo-2"; break;
            case Qt::Key_A: symbol="scan"; break;
            case Qt::Key_S: symbol="save"; break;
            }
        }
        break;
    }
    return symbolIcon(symbol,color);
}
}
