#pragma once
#include "BigPicture.h"
#include "Emulators.h"
#include "Igdb.h"
#include "Library.h"
#include "Metadata.h"
#include "Updater.h"
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
class QStackedWidget;
class Banner;
class BgWidget;

class MainWindow : public QMainWindow {
    Q_OBJECT
public:
    MainWindow();
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
    void openSettings();
    void refreshMetadata(bool force);
    void checkEmuUpdates();
    void gameMenu(const QPoint& pos);
    void fillLogoMenu();
    void toggleMaximize();
    void enterBigPicture();
    void leaveBigPicture();
    void toggleFullscreen();
    void showWelcome();
    void checkAppUpdate();
    void restartApp();
private:
    void openSettingsTab(int tab);
    void rebuildTree();
    void refreshStatus();
    void notify(const QString& title, const QString& msg);
    void applyGeneral();
    void rescanImpl(bool network);
    void applyLogo();
    void applyBackground();
    void gameSettings(const Game& g);
    void promptRestart();
    void askInstallUpdate(const QString& tag);
    void refreshBigPicture();
    void playGame(const Game& g);
    bool canPlay(const Game& g) const;
    const Game* current() const;
    const Game* gameByKey(const QString& key) const;

    EmulatorManager emu_;
    Igdb igdb_;
    Metadata meta_;
    Updater updater_;
    QList<Game> games_;
    QMap<QString, QTreeWidgetItem*> items_;
    QHash<QString, QIcon> icons_;
    QTreeWidget* tree_;
    Banner* banner_;
    BgWidget* bg_;
    QLabel *title_, *info_, *status_, *metaLbl_;
    QPushButton *playBtn_, *installBtn_;
    QTextBrowser* desc_;
    QProgressBar* metaBar_;
    QToolButton *maxBtn_ = nullptr, *logoBtn_ = nullptr;
    QMenu* logoMenu_;
    QStackedWidget* stack_;
    BigPicture* bp_;
    QSystemTrayIcon* tray_ = nullptr;
    QTimer *metaTimer_, *bpTimer_;
    bool frameless_ = true, quitting_ = false, shownTrayHint_ = false, manualUpdate_ = false;
};
