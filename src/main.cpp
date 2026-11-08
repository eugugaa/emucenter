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
#include <QThread>

int main(int argc, char** argv) {
    QApplication app(argc, argv);
    QApplication::setOrganizationName("EmuCenter");
    QApplication::setApplicationName("EmuCenter");
    I18n::setLang(QSettings().value("lang", "en").toString());

    const QString serverName = "EmuCenter-single-instance";
    if (QCoreApplication::arguments().contains("--restarted")) {
        for (int i = 0; i < 30; ++i) {
            QLocalSocket probe;
            probe.connectToServer(serverName);
            if (!probe.waitForConnected(150)) break;
            probe.abort();
            QThread::msleep(200);
        }
    }
    {
        QLocalSocket probe;
        probe.connectToServer(serverName);
        if (probe.waitForConnected(300)) {
            probe.write("show");
            probe.waitForBytesWritten(300);
            QMessageBox::warning(nullptr, "EmuCenter", T("O EmuCenter já está aberto. Vou mostrar a janela que já existe."));
            return 0;
        }
    }
    QLocalServer::removeServer(serverName);
    QLocalServer server;
    server.listen(serverName);

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

    QObject::connect(&server, &QLocalServer::newConnection, &w, [&server, &w] {
        while (QLocalSocket* c = server.nextPendingConnection()) c->deleteLater();
        w.showNormal();
        w.raise();
        w.activateWindow();
    });
    return app.exec();
}
