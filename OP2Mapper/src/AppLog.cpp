// AppLog: OP2Mapper.log next to the exe.

#include "AppLog.h"

#include "AppInfo.h"

#include <QCoreApplication>
#include <QDateTime>
#include <QDebug>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QMutex>
#include <QStringList>
#include <QSysInfo>

namespace {
QMutex g_mutex;
constexpr qint64 kMaxSize = 1024 * 1024;

QString logPath(const char *name)
{
    return QDir(QCoreApplication::applicationDirPath()).filePath(QString::fromLatin1(name));
}
} // namespace

namespace AppLog {

// Line breaks in a message become " | ", so every entry is one line.
void write(const QString &category, const QString &message)
{
    QString msg = message;
    msg.replace(QLatin1String("\r\n"), QLatin1String(" | "));
    msg.replace(QLatin1Char('\n'), QLatin1String(" | "));
    msg.replace(QLatin1Char('\r'), QLatin1String(" | "));
    const QString line = QDateTime::currentDateTime().toString(
                             QStringLiteral("yyyy-MM-dd HH:mm:ss.zzz"))
        + QStringLiteral("  [") + category + QStringLiteral("] ") + msg;

    QMutexLocker lock(&g_mutex);
    qDebug().noquote() << "LOG" << line;

    const QString path = logPath("OP2Mapper.log");
    const QFileInfo info(path);
    if (info.exists() && info.size() > kMaxSize) {
        const QString old = logPath("OP2Mapper.old.log");
        QFile::remove(old);
        QFile::rename(path, old);
    }
    QFile f(path);
    if (!f.open(QIODevice::WriteOnly | QIODevice::Append))
        return;
    f.write((line + QStringLiteral("\r\n")).toUtf8());
}

// First line of a session: version, Qt, bitness, Windows version, folder, arguments.
void writeStartup(const QStringList &arguments)
{
    const QString args = arguments.isEmpty() ? QStringLiteral("(none)")
                                             : arguments.join(QLatin1Char(' '));
    write(QStringLiteral("Start"),
          QStringLiteral("%1 %2 (Qt %3), %4-bit, %5 (kernel %6), folder %7, arguments: %8")
              .arg(QString::fromLatin1(AppInfo::DisplayName), QString::fromLatin1(AppInfo::Version),
                   QString::fromLatin1(qVersion()))
              .arg(sizeof(void *) * 8)
              .arg(QSysInfo::prettyProductName(), QSysInfo::kernelVersion(),
                   QDir::toNativeSeparators(QCoreApplication::applicationDirPath()), args));
}

} // namespace AppLog
