#pragma once

#include <QString>

namespace Settings {
    QString op2Folder();
    void setOp2Folder(const QString &path);
    QString lastMapDir();
    void setLastMapDir(const QString &path);
}
