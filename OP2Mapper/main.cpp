// Entry point: single-instance check, log, settings migration, splash
// screen, main window, and the local server that receives a second launch's
// arguments.

#include <QApplication>
#include <QDataStream>
#include <QDir>
#include <QElapsedTimer>
#include <QIcon>
#include <QLocalServer>
#include <QLocalSocket>
#include <QPainter>
#include <QSplashScreen>
#include <QTimer>

#include "AppInfo.h"
#include "AppLog.h"
#include "MainWindow.h"
#include "OP2Launcher.h"
#include "Op2Lib.h"
#include "Settings.h"

#include <exception>

namespace {

// One running copy per Windows user.
QString instanceName()
{
    const QString user = qEnvironmentVariable("USERNAME", qEnvironmentVariable("USER"));
    return QStringLiteral("OP2Mapper-") + user;
}

// Single instance: a second launch forwards its arguments and exits.
bool forwardToRunningInstance(const QStringList &args)
{
    QLocalSocket socket;
    socket.connectToServer(instanceName());
    if (!socket.waitForConnected(300))
        return false;
    QByteArray data;
    QDataStream out(&data, QIODevice::WriteOnly);
    out << args;
    socket.write(data);
    socket.waitForBytesWritten(1000);
    socket.disconnectFromServer();
    return true;
}

// OP2Mapper3's splash text, drawn in code.
QPixmap splashPixmap()
{
    QPixmap pm(496, 303);
    pm.fill(QColor(240, 240, 240));
    QPainter p(&pm);
    p.setPen(QColor(160, 160, 160));
    p.drawRect(0, 0, 495, 302);
    QPixmap icon(QStringLiteral(":/images/IconLarge.ico"));
    if (!icon.isNull())
        p.drawPixmap(QRect(56, 72, 128, 128), icon);
    QFont title(QStringLiteral("MS Sans Serif"));
    title.setPixelSize(24);
    p.setFont(title);
    p.setPen(Qt::black);
    p.drawText(QRect(249, 20, 240, 192), Qt::AlignLeft | Qt::AlignBottom, QString::fromLatin1(AppInfo::DisplayName));
    QFont small(QStringLiteral("Segoe UI"));
    small.setPixelSize(12);
    p.setFont(small);
    p.drawText(QPoint(249, 236), QStringLiteral("Version %1").arg(QString::fromLatin1(AppInfo::Version)));
    p.drawText(QPoint(249, 256), QStringLiteral("Copyright © Outpost Universe"));
    return pm;
}

// Logs exceptions that escape an event handler instead of letting them end the app silently.
class App : public QApplication
{
public:
    using QApplication::QApplication;
    bool notify(QObject *receiver, QEvent *event) override
    {
        try {
            return QApplication::notify(receiver, event);
        } catch (const std::exception &e) {
            AppLog::write(QStringLiteral("Crash"), QStringLiteral("Unhandled exception: %1").arg(QString::fromUtf8(e.what())));
        } catch (...) {
            AppLog::write(QStringLiteral("Crash"), QStringLiteral("Unhandled exception of unknown type"));
        }
        return false;
    }
};

} // namespace

int main(int argc, char *argv[])
{
    App app(argc, argv);
    QCoreApplication::setOrganizationName(QStringLiteral("OutpostUniverse"));
    QCoreApplication::setApplicationName(QString::fromLatin1(AppInfo::Name));
    QCoreApplication::setApplicationVersion(QString::fromLatin1(AppInfo::Version));
    QApplication::setWindowIcon(QIcon(QStringLiteral(":/images/IconLarge.ico")));

    QStringList args = QCoreApplication::arguments();
    args.removeFirst();

    if (forwardToRunningInstance(args))
        return 0;

    AppLog::writeStartup(args);
    Settings::migrateLegacySettings();

    QSplashScreen splash(splashPixmap());
    splash.show();
    Sounds::playResource(QStringLiteral(":/sounds/earthwrk.wav"));
    app.processEvents();
    QElapsedTimer t;
    t.start();
    while (t.elapsed() < 600)
        app.processEvents(QEventLoop::AllEvents, 50);

    MainWindow window;
    splash.finish(&window);

    QLocalServer server;
    QLocalServer::removeServer(instanceName());
    server.listen(instanceName());
    QObject::connect(&server, &QLocalServer::newConnection, &window, [&] {
        while (QLocalSocket *s = server.nextPendingConnection()) {
            QObject::connect(s, &QLocalSocket::readyRead, &window, [s, &window] {
                QDataStream in(s);
                QStringList forwarded;
                in >> forwarded;
                window.handleSecondInstance(forwarded);
                s->deleteLater();
            });
        }
    });

    if (!window.startup(args)) {
        AppLog::write(QStringLiteral("Stop"), QStringLiteral("No Outpost 2 folder - exiting"));
        return 0;
    }
    const int rc = app.exec();
    Op2Lib::shutdown();
    AppLog::write(QStringLiteral("Stop"), QStringLiteral("OP2Mapper closed"));
    return rc;
}
