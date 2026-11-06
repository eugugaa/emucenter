#pragma once
#include "Emulators.h"
#include "Igdb.h"
#include "Library.h"
#include "Metadata.h"
#include <QHash>
#include <QIcon>
#include <QMainWindow>
#include <QMap>

class QTreeWidget;
class QTreeWidgetItem;
class QLabel;
class QPushButton;
class QToolButton;
class QTextBrowser;
class QProgressBar;
class QSystemTrayIcon;
class QTimer;
class QMenu;
class Banner;

class MainWindow : public QMainWindow {
    Q_OBJECT
public:
    MainWindow();
    void bringToFront();   // usado quando outra instância tenta abrir
protected:
    void closeEvent(QCloseEvent*) override;
    bool eventFilter(QObject*, QEvent*) override;
private slots:
    void addRomFolder();
    void importRoms();
    void rescan();
    void onSelect();
    void play();
    void installMissing();
    void openSettings(int tab = 0);
    void refreshMetadata(bool force);
    void checkEmuUpdates();
    void gameMenu(const QPoint& pos);
    void fillLogoMenu();
    void toggleMaximize();
private:
    void rebuildTree();
    void refreshStatus();
    void notify(const QString& title, const QString& msg);
    void applyGeneral();
    void rescanImpl(bool network);
    void applyLogo();
    void gameSettings(const Game& g);
    const Game* current() const;

    EmulatorManager emu_;
    Igdb igdb_;
    Metadata meta_;
    QList<Game> games_;
    QMap<QString, QTreeWidgetItem*> items_;
    QHash<QString, QIcon> icons_;
    QTreeWidget* tree_;
    Banner* banner_;
    QLabel *title_, *info_, *status_, *metaLbl_;
    QPushButton *playBtn_, *installBtn_;
    QTextBrowser* desc_;
    QProgressBar* metaBar_;
    QToolButton *maxBtn_ = nullptr, *logoBtn_ = nullptr;
    QMenu* logoMenu_;
    QSystemTrayIcon* tray_ = nullptr;
    QTimer* metaTimer_;
    bool frameless_ = true, quitting_ = false, shownTrayHint_ = false;
};
