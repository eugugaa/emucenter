#include "I18n.h"
#include "MainWindow.h"
#include "Theme.h"
#include <QApplication>
#include <QDir>
#include <QFile>
#include <QLocalServer>
#include <QLocalSocket>
#include <QMessageBox>
#include <QSettings>
#include <QSystemTrayIcon>

int main(int argc, char** argv) {
    QApplication app(argc, argv);
    QApplication::setOrganizationName("EmuCenter");
    QApplication::setApplicationName("EmuCenter");
    const QString lang = QSettings().value("lang", "pt").toString();
    I18n::setLang(lang);

    // ---- single instance: if an EmuCenter is already open, warns and brings it to the front ----
    const QString serverName = "EmuCenter-single-instance";
    {
        QLocalSocket probe;
        probe.connectToServer(serverName);
        if (probe.waitForConnected(300)) {
            probe.write("show");
            probe.waitForBytesWritten(300);
            QMessageBox::warning(nullptr, "EmuCenter",
                                 lang == "en" ? "EmuCenter is already running."
                                              : "O EmuCenter já está aberto.");
            return 0;
        }
    }
    QLocalServer::removeServer(serverName);   // clears leftovers from an unexpected previous close
    QLocalServer server;
    server.listen(serverName);

    // first run with no themes on disk: copies embedded examples to themes/all and themes/normal
    if (Theme::list(Theme::All).isEmpty() && Theme::list(Theme::Normal).isEmpty()) {
        const QList<QPair<Theme::Kind, QString>> ex = {{Theme::All, "exemplo-tudo.css"},
                                                       {Theme::Normal, "exemplo-botoes-arredondados.css"},
                                                       {Theme::Normal, "exemplo-verde-neon.css"}};
        for (const auto& e : ex) {
            QFile in(QString(":/themes/") + (e.first == Theme::All ? "all/" : "normal/") + e.second);
            if (!in.open(QIODevice::ReadOnly)) continue;
            QDir().mkpath(Theme::importDir(e.first));
            QFile out(Theme::importDir(e.first) + "/" + e.second);
            if (out.open(QIODevice::WriteOnly)) out.write(in.readAll());
        }
    }
    Theme::apply();

    MainWindow w;
    const bool hidden = QSettings().value("startMinimized", false).toBool() && QSystemTrayIcon::isSystemTrayAvailable();
    if (!hidden) w.show();

    // another instance tried to open: shows this window (also exits the tray)
    QObject::connect(&server, &QLocalServer::newConnection, &w, [&server, &w] {
        while (QLocalSocket* c = server.nextPendingConnection()) c->deleteLater();
        w.showNormal();
        w.raise();
        w.activateWindow();
    });
    return app.exec();
}