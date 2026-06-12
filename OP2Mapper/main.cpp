#include <QApplication>
#include <QDir>
#include <QIcon>

#include "MainWindow.h"
#include "Settings.h"

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    QCoreApplication::setOrganizationName("OutpostUniverse");
    QCoreApplication::setApplicationName("OP2Mapper");
    QApplication::setWindowIcon(QIcon(":/icons/app.ico"));

    // Ensure user-content folders exist on first run. Settings has the
    // configured path (default <app dir>/Tilegroups); create it + the
    // standard Exported subfolder so tile-group export and the Tile
    // Groups palette have a real folder to read/write.
    const QString tg = Settings::tileGroupsPath();
    QDir().mkpath(tg);
    QDir().mkpath(tg + QStringLiteral("/Exported"));

    MainWindow window;
    window.show();
    return app.exec();
}
