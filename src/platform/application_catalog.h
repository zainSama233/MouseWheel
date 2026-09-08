#pragma once
#include <QStringList>
#include <QList>
#include <QMetaType>
#include <atomic>
namespace wheel {
struct ApplicationEntry {
    QString name;
    QString path;
    QString executable;
    bool operator==(const ApplicationEntry&) const = default;
};
namespace win {
QStringList applicationShortcutRoots();
QList<ApplicationEntry> discoverApplications(const QStringList& shortcutRoots,const std::atomic_bool* cancelled=nullptr);
QList<ApplicationEntry> runningApplications();
}
}
Q_DECLARE_METATYPE(wheel::ApplicationEntry)
