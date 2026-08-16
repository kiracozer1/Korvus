#include <unistd.h>
#include <cstdlib>

#include <QApplication>
#include <QProcess>
#include <QMessageBox>
#include <QStyleFactory>
#include <QSplashScreen>
#include <QPixmap>
#include <QIcon>
#include <QScreen>
#include "mainwindow.h"

namespace {
QStringList buildPkexecArgs()
{
    QStringList args = {QStringLiteral("env")};
    const QList<QByteArray> passthroughVars = {
        "DISPLAY", "XAUTHORITY", "WAYLAND_DISPLAY", "XDG_RUNTIME_DIR", "XDG_SESSION_TYPE"};
    for (const QByteArray &name : passthroughVars) {
        if (qEnvironmentVariableIsSet(name.constData())) {
            const QString value = QString::fromLocal8Bit(qgetenv(name.constData()));
            args << (QString::fromLatin1(name) + QStringLiteral("=") + value);
        }
    }
    args << QCoreApplication::applicationFilePath();
    return args;
}
}

int main(int argc, char *argv[])
{
      qputenv("QT_QPA_PLATFORM", "xcb");
    QApplication app(argc, argv);
    QCoreApplication::setApplicationName(QStringLiteral("Korvus"));
    QCoreApplication::setOrganizationName(QStringLiteral("Korvus"));

    app.setStyle(QStyleFactory::create("Fusion"));
    app.setWindowIcon(QIcon(QStringLiteral(":/app_icon.png")));

    if (geteuid() != 0) {
        const bool started = QProcess::startDetached(QStringLiteral("pkexec"), buildPkexecArgs());
        if (!started) {
            QMessageBox::critical(nullptr, QStringLiteral("Korvus"),
                                   QStringLiteral("Korvus root yetkisi gerektirir ve pkexec bulunamadı.\n"
                                                  "Lütfen terminal üzerinden yetki vererek çalıştırın."));
        }
        return 0;
    }
const QPixmap splashImage(QStringLiteral(":/splash.png"));
    QSplashScreen splash(splashImage);

    if (QScreen *screen = QGuiApplication::primaryScreen()) {
        const QRect screenGeometry = screen->geometry();
        const QPoint center(screenGeometry.center().x() - splashImage.width() / 2,
                             screenGeometry.center().y() - splashImage.height() / 2);
        splash.move(center);
    }

    splash.show();
    app.processEvents();
    MainWindow window;
    splash.finish(&window);
    window.show();

    return app.exec();
}