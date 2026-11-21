#include "Settings.h"

#include "AppLog.h"
#include "IniFile.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QSettings>

namespace {
const QString kPaths = QStringLiteral("Paths");
const QString kWindow = QStringLiteral("Window");
constexpr int kMaxRecent = 5;

QString readPath(const QString &key)
{
    return IniFile::value(kPaths, key).trimmed();
}

void writePath(const QString &key, const QString &value)
{
    const QString v = value.trimmed();
    if (IniFile::contains(kPaths, key) && readPath(key) == v)
        return;
    IniFile::setValue(kPaths, key, v);
    AppLog::write(QStringLiteral("Settings"),
                  QStringLiteral("%1 set to: %2").arg(key, v.isEmpty() ? QStringLiteral("(empty)") : v));
}
} // namespace

namespace Settings {

void migrateLegacySettings()
{
    // The Qt port used to keep its settings in the registry (QSettings,
    // org "OutpostUniverse", app "OP2Mapper"). Copy any value whose INI key
    // is absent, once.
    QSettings old(QStringLiteral("OutpostUniverse"), QStringLiteral("OP2Mapper"));
    const struct { const char *oldKey; const char *iniKey; } paths[] = {
        { "op2Folder", "OP2Path" },
        { "workingPath", "WorkingPath" },
        { "tileGroupsPath", "Tilegroups" },
    };
    for (const auto &p : paths) {
        const QString iniKey = QString::fromLatin1(p.iniKey);
        if (IniFile::contains(kPaths, iniKey))
            continue;
        const QString v = QDir::toNativeSeparators(old.value(QString::fromLatin1(p.oldKey)).toString().trimmed());
        if (v.isEmpty())
            continue;
        IniFile::setValue(kPaths, iniKey, v);
        AppLog::write(QStringLiteral("Settings"),
                      QStringLiteral("Moved %1 from the old Qt registry settings to Mapper3.ini: %2").arg(iniKey, v));
    }
    if (!IniFile::contains(kWindow, QStringLiteral("RecentFile1"))) {
        const QStringList recent = old.value(QStringLiteral("recentFiles")).toStringList();
        for (int i = 0; i < kMaxRecent; ++i)
            IniFile::setValue(kWindow, QStringLiteral("RecentFile%1").arg(i + 1),
                              i < recent.size() ? QDir::toNativeSeparators(recent[i]) : QString());
    }
}

QString op2Path() { return readPath(QStringLiteral("OP2Path")); }
void setOp2Path(const QString &path) { writePath(QStringLiteral("OP2Path"), QDir::toNativeSeparators(path)); }
QString workingPath() { return readPath(QStringLiteral("WorkingPath")); }
void setWorkingPath(const QString &path) { writePath(QStringLiteral("WorkingPath"), QDir::toNativeSeparators(path)); }
QString tileGroupsPath() { return readPath(QStringLiteral("Tilegroups")); }
void setTileGroupsPath(const QString &path) { writePath(QStringLiteral("Tilegroups"), QDir::toNativeSeparators(path)); }

QString defaultTileGroupsPath()
{
    return QDir::toNativeSeparators(
        QDir(QCoreApplication::applicationDirPath()).filePath(QStringLiteral("Tilegroups")));
}

QStringList recentFiles()
{
    QStringList list;
    for (int i = 1; i <= kMaxRecent; ++i) {
        const QString p = IniFile::value(kWindow, QStringLiteral("RecentFile%1").arg(i)).trimmed();
        if (!p.isEmpty() && QFile::exists(p))
            list.append(p);
    }
    return list;
}

static void saveRecent(const QStringList &list)
{
    for (int i = 0; i < kMaxRecent; ++i)
        IniFile::setValue(kWindow, QStringLiteral("RecentFile%1").arg(i + 1),
                          i < list.size() ? list[i] : QString());
}

void addRecentFile(const QString &path)
{
    const QString p = QDir::toNativeSeparators(path);
    QStringList list = recentFiles();
    for (int i = list.size() - 1; i >= 0; --i) {
        if (list[i].compare(p, Qt::CaseInsensitive) == 0)
            list.removeAt(i);
    }
    list.prepend(p);
    while (list.size() > kMaxRecent)
        list.removeLast();
    saveRecent(list);
}

void removeRecentFile(const QString &path)
{
    QStringList list;
    for (int i = 1; i <= kMaxRecent; ++i) {
        const QString p = IniFile::value(kWindow, QStringLiteral("RecentFile%1").arg(i)).trimmed();
        if (!p.isEmpty() && p.compare(QDir::toNativeSeparators(path), Qt::CaseInsensitive) != 0)
            list.append(p);
    }
    saveRecent(list);
}

bool pasteCellTypes()
{
    return IniFile::value(kWindow, QStringLiteral("PasteCellTypes"), QStringLiteral("True"))
               .trimmed().compare(QLatin1String("False"), Qt::CaseInsensitive) != 0;
}

void setPasteCellTypes(bool on) { IniFile::setBool(kWindow, QStringLiteral("PasteCellTypes"), on); }

bool allowEditLastRow() { return IniFile::boolValue(kWindow, QStringLiteral("AllowEditLastRow"), false); }

QString windowValue(const QString &key, const QString &defaultValue) { return IniFile::value(kWindow, key, defaultValue); }
void setWindowValue(const QString &key, const QString &value) { IniFile::setValue(kWindow, key, value); }
bool windowBool(const QString &key, bool defaultValue) { return IniFile::boolValue(kWindow, key, defaultValue); }
void setWindowBool(const QString &key, bool v) { IniFile::setBool(kWindow, key, v); }
int windowInt(const QString &key, int defaultValue) { return IniFile::intValue(kWindow, key, defaultValue); }
void setWindowInt(const QString &key, int v) { IniFile::setInt(kWindow, key, v); }
bool hasWindowValue(const QString &key) { return IniFile::contains(kWindow, key); }

bool isValidOp2Folder(const QString &path)
{
    if (path.trimmed().isEmpty())
        return false;
    const QDir d(path);
    return d.exists() && QFile::exists(d.filePath(QStringLiteral("Outpost2.exe")))
        && QFile::exists(d.filePath(QStringLiteral("OPU/OPULauncher.exe")));
}

} // namespace Settings
