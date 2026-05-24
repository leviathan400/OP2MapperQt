#include "Settings.h"

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

}
