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

void writeStartup(const QStringList &arguments)
{
    const QString args = arguments.isEmpty() ? QStringLiteral("(none)")
                                             : arguments.join(QLatin1Char(' '));
    write(QStringLiteral("Start"),
          QStringLiteral("%1 %2 build %3 (Qt %4), %5-bit, %6 (kernel %7), folder %8, arguments: %9")
              .arg(QString::fromLatin1(AppInfo::Name), QString::fromLatin1(AppInfo::Version),
                   QString::fromLatin1(AppInfo::Build), QString::fromLatin1(qVersion()))
              .arg(sizeof(void *) * 8)
              .arg(QSysInfo::prettyProductName(), QSysInfo::kernelVersion(),
                   QDir::toNativeSeparators(QCoreApplication::applicationDirPath()), args));
}

} // namespace AppLog
