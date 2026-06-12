#include "Settings.h"

#include <QCoreApplication>
#include <QDir>
#include <QSettings>

namespace Settings {

QString op2Folder() {
    return QSettings().value("op2Folder").toString();
}

void setOp2Folder(const QString &path) {
    QSettings().setValue("op2Folder", path);
}

QString lastMapDir() {
    return QSettings().value("lastMapDir").toString();
}

void setLastMapDir(const QString &path) {
    QSettings().setValue("lastMapDir", path);
}

QString tileGroupsPath() {
    const QString stored = QSettings().value("tileGroupsPath").toString();
    if (!stored.isEmpty())
        return stored;
    return QDir(QCoreApplication::applicationDirPath()).filePath("Tilegroups");
}

void setTileGroupsPath(const QString &path) {
    QSettings().setValue("tileGroupsPath", path);
}

QString workingPath() {
    const QString stored = QSettings().value("workingPath").toString();
    if (!stored.isEmpty())
        return stored;
    const QString last = lastMapDir();
    return last.isEmpty() ? QCoreApplication::applicationDirPath() : last;
}

void setWorkingPath(const QString &path) {
    QSettings().setValue("workingPath", path);
}

QStringList recentFiles() {
    return QSettings().value("recentFiles").toStringList();
}

void addRecentFile(const QString &path) {
    QStringList list = recentFiles();
    list.removeAll(path);
    list.prepend(path);
    while (list.size() > 5)
        list.removeLast();
    QSettings().setValue("recentFiles", list);
}

void removeRecentFile(const QString &path) {
    QStringList list = recentFiles();
    list.removeAll(path);
    QSettings().setValue("recentFiles", list);
}

}
