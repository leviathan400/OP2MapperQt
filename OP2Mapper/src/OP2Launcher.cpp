#include "OP2Launcher.h"

#include "AppLog.h"
#include "Settings.h"

#include <QCryptographicHash>
#include <QDesktopServices>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QMessageBox>
#include <QProcess>
#include <QString>
#include <QUrl>

#ifdef Q_OS_WIN
#include <windows.h>
#include <mmsystem.h>
#endif

namespace {

bool checkOp2Path(QWidget *parent, const QString &title)
{
    const QString op2 = Settings::op2Path();
    if (!op2.isEmpty() && QDir(op2).exists())
        return true;
    QMessageBox::warning(parent, title,
        QObject::tr("Outpost 2 game directory is not set or doesn't exist.\n\nConfigure it in File → Settings."));
    return false;
}

QString md5Hex(const QByteArray &bytes)
{
    const QByteArray h = QCryptographicHash::hash(bytes, QCryptographicHash::Md5).toHex().toUpper();
    QString out;
    for (int i = 0; i < h.size(); i += 2) {
        if (i)
            out += QLatin1Char('-');
        out += QString::fromLatin1(h.mid(i, 2));
    }
    return out;
}

bool writeFile(const QString &path, const QByteArray &bytes, QString &error)
{
    QFile f(path);
    if (!f.open(QIODevice::WriteOnly | QIODevice::Truncate) || f.write(bytes) != bytes.size()) {
        error = f.errorString();
        return false;
    }
    AppLog::write(QStringLiteral("Launcher"),
                  QStringLiteral("Installed %1 (md5 %2)").arg(QDir::toNativeSeparators(path), md5Hex(bytes)));
    return true;
}

bool ensureInstalled(QWidget *parent, const QString &title)
{
    if (!checkOp2Path(parent, title))
        return false;
    const QString opu = QDir(Settings::op2Path()).filePath(QStringLiteral("OPU"));
    if (!QDir(opu).exists()) {
        QMessageBox::warning(parent, title,
            QObject::tr("The OPU folder was not found:\n%1\n\nOP2Launcher needs an Outpost 2 install with the OPU update.")
                .arg(QDir::toNativeSeparators(opu)));
        return false;
    }
    const QString exePath = QDir(opu).filePath(QStringLiteral("op2launcher.exe"));
    const QString dllPath = QDir(opu).filePath(QStringLiteral("Launcher.dll"));
    QFile exeRes(QStringLiteral(":/op2launcher/op2launcher.exe"));
    QFile dllRes(QStringLiteral(":/op2launcher/Launcher.dll"));
    if (!exeRes.open(QIODevice::ReadOnly) || !dllRes.open(QIODevice::ReadOnly)) {
        AppLog::write(QStringLiteral("Launcher"), QStringLiteral("Bundled op2launcher files could not be read"));
        return QFile::exists(exePath);
    }
    const QByteArray exeBytes = exeRes.readAll();
    const QByteArray dllBytes = dllRes.readAll();

    bool installExe = false, installDll = false;
    if (!QFile::exists(exePath)) {
        AppLog::write(QStringLiteral("Launcher"),
                      QStringLiteral("op2launcher.exe not installed in %1 - installing the bundled copy").arg(QDir::toNativeSeparators(opu)));
        installExe = installDll = true;
    } else {
        QFile cur(exePath);
        QByteArray curBytes;
        if (cur.open(QIODevice::ReadOnly))
            curBytes = cur.readAll();
        if (QCryptographicHash::hash(curBytes, QCryptographicHash::Md5)
            == QCryptographicHash::hash(exeBytes, QCryptographicHash::Md5)) {
            installDll = !QFile::exists(dllPath);
        } else {
            const auto choice = QMessageBox::question(parent, title,
                QObject::tr("A different version of op2launcher.exe is installed in:\n%1\n\n"
                            "Replace it with the version bundled with OP2Mapper?\n\n"
                            "Choose No to keep using the installed one.").arg(QDir::toNativeSeparators(opu)),
                QMessageBox::Yes | QMessageBox::No | QMessageBox::Cancel);
            AppLog::write(QStringLiteral("Launcher"),
                          QStringLiteral("Different op2launcher.exe in %1 - user chose %2").arg(
                              QDir::toNativeSeparators(opu),
                              choice == QMessageBox::Yes ? QStringLiteral("Yes")
                              : choice == QMessageBox::No ? QStringLiteral("No") : QStringLiteral("Cancel")));
            if (choice == QMessageBox::Cancel)
                return false;
            if (choice == QMessageBox::No)
                return true;
            installExe = installDll = true;
        }
    }
    QString err;
    if ((installExe && !writeFile(exePath, exeBytes, err)) || (installDll && !writeFile(dllPath, dllBytes, err))) {
        QMessageBox::critical(parent, title,
            QObject::tr("OP2Launcher could not be installed into:\n%1\n\n%2\n\nIf Outpost 2 is under Program Files, "
                        "run OP2Mapper as administrator once, or close Outpost 2 if it is running, then try again.")
                .arg(QDir::toNativeSeparators(opu), err));
        return false;
    }
    return true;
}

} // namespace

namespace OP2Launcher {

void launchOutpost2(QWidget *parent)
{
    const QString title = QObject::tr("Launch Outpost 2");
    if (!checkOp2Path(parent, title))
        return;
    const QString exe = QDir(Settings::op2Path()).filePath(QStringLiteral("OPU/OPULauncher.exe"));
    if (!QFile::exists(exe)) {
        QMessageBox::critical(parent, title,
            QObject::tr("OPULauncher.exe was not found at:\n%1\n\nCheck that the Outpost 2 directory in Settings points to an OPU install.")
                .arg(QDir::toNativeSeparators(exe)));
        return;
    }
    if (!QProcess::startDetached(exe, {}, QFileInfo(exe).absolutePath())) {
        QMessageBox::critical(parent, title, QObject::tr("Failed to launch Outpost 2: %1").arg(QObject::tr("the process could not be started.")));
        return;
    }
    AppLog::write(QStringLiteral("Launch"), QStringLiteral("Started Outpost 2: %1").arg(QDir::toNativeSeparators(exe)));
}

void launchDllPicker(QWidget *parent)
{
    const QString title = QObject::tr("Launch DLL Picker");
    if (!ensureInstalled(parent, title))
        return;
    const QString opu = QDir(Settings::op2Path()).filePath(QStringLiteral("OPU"));
    const QString exe = QDir(opu).filePath(QStringLiteral("op2launcher.exe"));
    if (!QFile::exists(exe)) {
        QMessageBox::critical(parent, title,
            QObject::tr("op2launcher.exe was not found at:\n%1").arg(QDir::toNativeSeparators(exe)));
        return;
    }
    if (!QProcess::startDetached(exe, { QStringLiteral("--picker") }, opu)) {
        QMessageBox::critical(parent, title, QObject::tr("Failed to launch: %1").arg(QObject::tr("the process could not be started.")));
        return;
    }
    AppLog::write(QStringLiteral("Launch"), QStringLiteral("Started op2launcher.exe --picker"));
}

void openGameFolder(QWidget *parent)
{
    const QString title = QObject::tr("Open Outpost 2 Folder");
    if (!checkOp2Path(parent, title))
        return;
    if (!QDesktopServices::openUrl(QUrl::fromLocalFile(Settings::op2Path())))
        QMessageBox::critical(parent, title,
            QObject::tr("Failed to open Outpost 2 folder: %1").arg(QDir::toNativeSeparators(Settings::op2Path())));
}

} // namespace OP2Launcher

namespace Sounds {

void playFile(const QString &path)
{
#ifdef Q_OS_WIN
    if (path.isEmpty() || !QFile::exists(path))
        return;
    const std::wstring w = QDir::toNativeSeparators(path).toStdWString();
    PlaySoundW(w.c_str(), nullptr, SND_FILENAME | SND_ASYNC | SND_NODEFAULT);
#else
    Q_UNUSED(path);
#endif
}

void playResource(const QString &resourcePath)
{
#ifdef Q_OS_WIN
    static QByteArray data; // must outlive the asynchronous playback
    QFile f(resourcePath);
    if (!f.open(QIODevice::ReadOnly))
        return;
    data = f.readAll();
    PlaySoundW(reinterpret_cast<LPCWSTR>(data.constData()), nullptr, SND_MEMORY | SND_ASYNC | SND_NODEFAULT);
#else
    Q_UNUSED(resourcePath);
#endif
}

} // namespace Sounds
