#include <QApplication>
#include <QIcon>

#include "MainWindow.h"

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    QCoreApplication::setOrganizationName("OutpostUniverse");
    QCoreApplication::setApplicationName("OP2Mapper");
    QApplication::setWindowIcon(QIcon(":/icons/app.ico"));

    MainWindow window;
    window.show();
    return app.exec();
}
