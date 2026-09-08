#include "platform/application_catalog.h"
#include "platform/program_icon.h"
#include <QDirIterator>
#include <QFileInfo>
#include <QSet>
#import <AppKit/AppKit.h>
namespace wheel::platform {
QStringList applicationShortcutRoots(){return {"/Applications","/System/Applications",QDir::homePath()+"/Applications"};}
QList<ApplicationEntry> discoverApplications(const QStringList& roots,const std::atomic_bool* cancelled) {
    QList<ApplicationEntry> result;QSet<QString> seen;
    QStringList pending=roots;
    while(!pending.isEmpty()) {
        if(cancelled && cancelled->load())return {};
        const auto root=pending.takeFirst();
        for(const auto& entry:QDir(root).entryInfoList(QDir::Dirs|QDir::NoDotAndDotDot|QDir::NoSymLinks)) {
            const auto path=entry.absoluteFilePath();
            if(entry.suffix()=="app"){if(!seen.contains(path)){seen.insert(path);result.append({entry.completeBaseName(),path,path});}}
            else pending.append(path);
        }
    }return result;
}
QList<ApplicationEntry> runningApplications() {
    @autoreleasepool {QList<ApplicationEntry> result;
        for(NSRunningApplication* app in NSWorkspace.sharedWorkspace.runningApplications){if(app.processIdentifier==NSProcessInfo.processInfo.processIdentifier || app.activationPolicy!=NSApplicationActivationPolicyRegular || !app.bundleURL)continue;
            const auto path=QString::fromNSString(app.bundleURL.path);result.append({QString::fromNSString(app.localizedName),path,path});}return result;}
}
QImage programIcon(const QString& path) {
    @autoreleasepool {NSImage* icon=[NSWorkspace.sharedWorkspace iconForFile:path.toNSString()];NSBitmapImageRep* bitmap=[NSBitmapImageRep imageRepWithData:icon.TIFFRepresentation];NSData* data=[bitmap representationUsingType:NSBitmapImageFileTypePNG properties:@{}];
        return QImage::fromData(static_cast<const uchar*>(data.bytes),int(data.length));}
}
}
