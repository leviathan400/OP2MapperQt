#include "BlankMaps.h"

#include <QDir>
#include <QFile>
#include <QStandardPaths>

namespace {

const QVector<BlankMaps::Size> kSizes = {
    { 64,  64 },
    { 64,  128 },
    { 64,  256 },
    { 128, 64 },
    { 128, 128 },
    { 128, 256 },
    { 256, 64 },
    { 256, 128 },
    { 256, 256 },
    { 512, 256 },
};

QString extractDir()
{
    const QString base = QStandardPaths::writableLocation(
        QStandardPaths::AppLocalDataLocation);
    QDir().mkpath(base + QStringLiteral("/BlankMaps"));
    return base + QStringLiteral("/BlankMaps");
}

bool extractOnce(const QString &resourcePath, const QString &diskPath)
{
    if (QFile::exists(diskPath))
        return true;
    QFile src(resourcePath);
    if (!src.open(QIODevice::ReadOnly))
        return false;
    QFile dst(diskPath);
    if (!dst.open(QIODevice::WriteOnly | QIODevice::Truncate))
        return false;
    dst.write(src.readAll());
    return true;
}

}

namespace BlankMaps {

QVector<Size> availableSizes()
{
    return kSizes;
}

QString pathFor(int width, int height)
{
    bool match = false;
    for (const auto &s : kSizes) {
        if (s.width == width && s.height == height) { match = true; break; }
    }
    if (!match)
        return QString();

    const QString fileName = QStringLiteral("blank_%1x%2.map").arg(width).arg(height);
    const QString diskPath = extractDir() + QLatin1Char('/') + fileName;
    const QString rcPath = QStringLiteral(":/blankmaps/") + fileName;
    if (!extractOnce(rcPath, diskPath))
        return QString();
    return diskPath;
}

} // namespace BlankMaps
