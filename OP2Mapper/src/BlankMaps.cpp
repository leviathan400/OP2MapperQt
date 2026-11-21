// BlankMaps: extraction of the embedded blank .map templates used by File > New Map.

#include "BlankMaps.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QStandardPaths>
#include <QTemporaryFile>

#ifdef Q_OS_WIN
#include <windows.h>
#endif

namespace {

const QVector<BlankMaps::Size> kSizes = {
    { 64, 64 }, { 64, 128 }, { 64, 256 }, { 128, 64 }, { 128, 128 },
    { 128, 256 }, { 256, 64 }, { 256, 128 }, { 256, 256 }, { 512, 256 },
};

QString fileName(int w, int h) { return QStringLiteral("blank_%1x%2.map").arg(w).arg(h); }

// True if a file can be created in `dir` (decides between the exe folder and LOCALAPPDATA).
bool dirWritable(const QString &dir)
{
    QTemporaryFile probe(QDir(dir).filePath(QStringLiteral("probe_XXXXXX.tmp")));
    return probe.open();
}

} // namespace

namespace BlankMaps {

QVector<Size> availableSizes() { return kSizes; }

// <exe>/BlankMaps if it already has templates or the exe folder is writable,
// otherwise %LOCALAPPDATA%/OP2Mapper/BlankMaps (e.g. under Program Files).
QString folder()
{
    static QString cached;
    if (!cached.isEmpty())
        return cached;
    const QString exeDir = QCoreApplication::applicationDirPath();
    const QString local = QDir(exeDir).filePath(QStringLiteral("BlankMaps"));
    const bool hasTemplates = !QDir(local).entryList({ QStringLiteral("blank_*.map") }, QDir::Files).isEmpty();
    if (hasTemplates || dirWritable(exeDir)) {
        cached = local;
    } else {
        QString base = QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation);
        cached = QDir(base).filePath(QStringLiteral("OP2Mapper/BlankMaps"));
    }
    return cached;
}

// Writes every template that is missing or has the wrong size, through a
// .tmp file and a rename, so a truncated template is repaired. On Windows the
// folder is marked hidden when something was written, as OP2Mapper3 does.
bool ensureExtracted(QString &error)
{
    const QString dir = folder();
    if (!QDir().mkpath(dir)) {
        error = QStringLiteral("Could not create the folder.");
        return false;
    }
    bool wrote = false;
    for (const Size &s : kSizes) {
        const QString name = fileName(s.width, s.height);
        QFile src(QStringLiteral(":/blankmaps/") + name);
        if (!src.open(QIODevice::ReadOnly)) {
            error = QStringLiteral("The embedded template %1 is missing.").arg(name);
            return false;
        }
        const QString target = QDir(dir).filePath(name);
        if (QFileInfo(target).size() == src.size())
            continue;
        const QByteArray bytes = src.readAll();
        const QString tmp = target + QStringLiteral(".tmp");
        QFile out(tmp);
        if (!out.open(QIODevice::WriteOnly | QIODevice::Truncate) || out.write(bytes) != bytes.size()) {
            error = out.errorString();
            out.close();
            QFile::remove(tmp);
            return false;
        }
        out.close();
        QFile::remove(target);
        if (!QFile::rename(tmp, target)) {
            error = QStringLiteral("Could not write %1.").arg(QDir::toNativeSeparators(target));
            QFile::remove(tmp);
            return false;
        }
        wrote = true;
    }
#ifdef Q_OS_WIN
    if (wrote) {
        const std::wstring w = QDir::toNativeSeparators(dir).toStdWString();
        const DWORD attrs = GetFileAttributesW(w.c_str());
        if (attrs != INVALID_FILE_ATTRIBUTES)
            SetFileAttributesW(w.c_str(), attrs | FILE_ATTRIBUTE_HIDDEN);
    }
#else
    Q_UNUSED(wrote);
#endif
    return true;
}

// Extracts on demand if the file isn't there yet.
QString pathFor(int width, int height)
{
    bool match = false;
    for (const auto &s : kSizes)
        match = match || (s.width == width && s.height == height);
    if (!match)
        return QString();
    const QString p = QDir(folder()).filePath(fileName(width, height));
    if (!QFile::exists(p)) {
        QString err;
        if (!ensureExtracted(err))
            return QString();
    }
    return QFile::exists(p) ? p : QString();
}

} // namespace BlankMaps
