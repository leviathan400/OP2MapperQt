#include "SettingsDialog.h"

#include "AppInfo.h"
#include "Settings.h"

#include <QCoreApplication>
#include <QDesktopServices>
#include <QDir>
#include <QFileDialog>
#include <QGroupBox>
#include <QIcon>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPushButton>
#include <QStandardPaths>
#include <QUrl>

namespace {
QLabel *banner(QWidget *parent)
{
    auto *b = new QLabel(parent);
    b->setGeometry(20, 20, 620, 100);
    b->setStyleSheet(QStringLiteral("background: steelblue;"));
    b->setPixmap(QPixmap(QStringLiteral(":/images/banner03_620x100.png")));
    b->setScaledContents(true);
    return b;
}
QString versionText()
{
    return QStringLiteral("Version %1 (Build %2)").arg(QString::fromLatin1(AppInfo::Version),
                                                     QString::fromLatin1(AppInfo::Build));
}
} // namespace

SettingsDialog::SettingsDialog(QWidget *parent)
    : QDialog(parent)
{
    setWindowTitle(tr("Settings"));
    setWindowIcon(QIcon(QStringLiteral(":/images/IconLarge.ico")));
    setWindowFlags(windowFlags() & ~Qt::WindowContextHelpButtonHint);
    setFixedSize(664, 495);
    banner(this);

    auto *group = new QGroupBox(tr("Paths"), this);
    group->setGeometry(20, 140, 620, 290);
    auto row = [&](const QString &caption, int y, QLineEdit *&edit, const QString &value) {
        auto *l = new QLabel(caption, group);
        l->setGeometry(16, y, 500, 18);
        edit = new QLineEdit(value, group);
        edit->setGeometry(16, y + 22, 504, 24);
        auto *b = new QPushButton(tr("Browse..."), group);
        b->setGeometry(526, y + 21, 80, 26);
        return b;
    };
    QPushButton *b1 = row(tr("Outpost 2 Game Directory (1.4.1):"), 30, m_op2, Settings::op2Path());
    QPushButton *b2 = row(tr("Default Save Directory:"), 110, m_working, Settings::workingPath());
    QPushButton *b3 = row(tr("Tile Groups Directory:"), 190, m_tileGroups, Settings::tileGroupsPath());
    connect(b1, &QPushButton::clicked, this, &SettingsDialog::browseOp2);
    connect(b2, &QPushButton::clicked, this, [this] {
        browseFolder(m_working, tr("Select Default Save Directory"),
                     QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation));
    });
    connect(b3, &QPushButton::clicked, this, [this] {
        browseFolder(m_tileGroups, tr("Select Tile Groups Directory"), QCoreApplication::applicationDirPath());
    });

    auto *ver = new QLabel(versionText(), this);
    ver->setGeometry(20, 450, 300, 20);
    auto *save = new QPushButton(tr("Save"), this);
    save->setGeometry(470, 445, 80, 30);
    save->setDefault(true);
    auto *cancel = new QPushButton(tr("Cancel"), this);
    cancel->setGeometry(560, 445, 80, 30);
    connect(save, &QPushButton::clicked, this, &SettingsDialog::onSave);
    connect(cancel, &QPushButton::clicked, this, &QDialog::reject);
}

void SettingsDialog::browseOp2()
{
    QString start = m_op2->text().trimmed();
    if (start.isEmpty() || !QDir(start).exists())
        start = qEnvironmentVariable("ProgramFiles");
    for (;;) {
        const QString dir = QFileDialog::getExistingDirectory(this, tr("Select Outpost 2 Game Directory"), start,
                                                              QFileDialog::ShowDirsOnly);
        if (dir.isEmpty())
            return;
        if (Settings::isValidOp2Folder(dir)) {
            m_op2->setText(QDir::toNativeSeparators(dir));
            return;
        }
        QMessageBox::critical(this, tr("Invalid Outpost 2 Directory"),
            tr("The selected directory is not a valid Outpost 2 v1.4.1 installation.\n\nPlease ensure:\n"
               "- Outpost2.exe exists in the main directory\n- OPULauncher.exe exists in the /OPU subfolder"));
        start = dir;
    }
}

void SettingsDialog::browseFolder(QLineEdit *edit, const QString &title, const QString &fallback)
{
    QString start = edit->text().trimmed();
    if (start.isEmpty() || !QDir(start).exists())
        start = fallback;
    const QString dir = QFileDialog::getExistingDirectory(this, title, start);
    if (!dir.isEmpty())
        edit->setText(QDir::toNativeSeparators(dir));
}

bool SettingsDialog::checkOptionalFolder(QLineEdit *edit, const QString &description)
{
    const QString p = edit->text().trimmed();
    if (p.isEmpty() || QDir(p).exists())
        return true;
    if (QMessageBox::question(this, tr("Settings"),
            tr("The %1 doesn't exist:\n\n%2\n\nCreate it?").arg(description, p),
            QMessageBox::Yes | QMessageBox::No) != QMessageBox::Yes) {
        edit->setFocus();
        return false;
    }
    if (!QDir().mkpath(p)) {
        QMessageBox::critical(this, tr("Settings"),
            tr("Could not create the %1:\n\n%2").arg(description, tr("Access denied or invalid path.")));
        edit->setFocus();
        return false;
    }
    return true;
}

void SettingsDialog::onSave()
{
    const QString op2 = m_op2->text().trimmed();
    if (op2.isEmpty() || !QDir(op2).exists()) {
        QMessageBox::critical(this, tr("Invalid Outpost 2 Directory"),
            tr("The Outpost 2 directory doesn't exist:\n\n%1").arg(op2.isEmpty() ? tr("(empty)") : op2));
        m_op2->setFocus();
        return;
    }
    if (!Settings::isValidOp2Folder(op2)) {
        if (QMessageBox::warning(this, tr("Outpost 2 Directory"),
                tr("This folder doesn't look like an Outpost 2 v1.4.1 installation (expected Outpost2.exe, "
                   "and OPU\\OPULauncher.exe).\n\nTilesets may not load. Save it anyway?"),
                QMessageBox::Yes | QMessageBox::No) != QMessageBox::Yes) {
            m_op2->setFocus();
            return;
        }
    }
    if (!checkOptionalFolder(m_working, tr("default save directory")))
        return;
    if (!checkOptionalFolder(m_tileGroups, tr("tile groups directory")))
        return;
    Settings::setOp2Path(op2);
    Settings::setWorkingPath(m_working->text().trimmed());
    Settings::setTileGroupsPath(m_tileGroups->text().trimmed());
    accept();
}

void showAboutDialog(QWidget *parent)
{
    QDialog dlg(parent);
    dlg.setWindowTitle(QObject::tr("About OP2Mapper"));
    dlg.setWindowIcon(QIcon(QStringLiteral(":/images/IconLarge.ico")));
    dlg.setWindowFlags(dlg.windowFlags() & ~Qt::WindowContextHelpButtonHint);
    dlg.setFixedSize(664, 350);
    banner(&dlg);
    auto *name = new QLabel(QString::fromLatin1(AppInfo::Name), &dlg);
    QFont f = name->font();
    f.setPointSize(14);
    f.setBold(true);
    name->setFont(f);
    name->setGeometry(20, 140, 600, 28);
    auto *ver = new QLabel(versionText() + QStringLiteral("  —  Qt %1").arg(QString::fromLatin1(qVersion())), &dlg);
    ver->setGeometry(22, 170, 600, 22);
    auto *desc = new QLabel(QString::fromLatin1(AppInfo::Description), &dlg);
    desc->setGeometry(22, 200, 618, 40);
    desc->setWordWrap(true);
    auto *copy = new QLabel(QString::fromUtf8(AppInfo::Copyright), &dlg);
    copy->setStyleSheet(QStringLiteral("color: gray;"));
    copy->setGeometry(22, 250, 600, 18);
    auto *link = new QLabel(QStringLiteral("<a href=\"%1\">%1</a>").arg(QString::fromLatin1(AppInfo::Website)), &dlg);
    link->setGeometry(22, 270, 600, 18);
    link->setOpenExternalLinks(true);
    auto *ok = new QPushButton(QObject::tr("OK"), &dlg);
    ok->setGeometry(560, 300, 80, 30);
    ok->setDefault(true);
    QObject::connect(ok, &QPushButton::clicked, &dlg, &QDialog::accept);
    dlg.exec();
}
