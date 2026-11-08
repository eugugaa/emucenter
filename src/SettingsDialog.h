#pragma once
#include "Emulators.h"
#include "Igdb.h"
#include "Metadata.h"
#include "Updater.h"
#include <QDialog>
#include <QMap>

class QTabWidget;
class QTableWidget;
class QListWidget;
class QProgressBar;
class QLabel;
class QGridLayout;
class QPushButton;

class SettingsDialog : public QDialog {
    Q_OBJECT
public:
    enum Tab { General, Folders, Emus, Appearance, Meta, Advanced };
    SettingsDialog(EmulatorManager* emu, Metadata* meta, Igdb* igdb, Updater* updater, QWidget* parent, int tab = General);
signals:
    void romFoldersChanged();
    void appearanceChanged();
    void generalChanged();
    void metadataRequested(bool force);
    void emulatorsChanged();
    void languageChanged();
    void welcomeRequested();
private:
    QWidget* buildGeneral();
    QWidget* buildFolders();
    QWidget* buildEmus();
    QWidget* buildAppearance();
    QWidget* buildMeta();
    QWidget* buildAdvanced();
    void refreshEmuTable();
    void refreshThemeButtons();
    void refreshRomList();
    void applyUi();

    EmulatorManager* emu_;
    Metadata* meta_;
    Igdb* igdb_;
    Updater* updater_;
    QTabWidget* tabs_;
    QTableWidget* emuTable_ = nullptr;
    QListWidget* romList_ = nullptr;
    QLabel* rootLbl_ = nullptr;
    QLabel* cacheLbl_ = nullptr;
    QLabel* updLbl_ = nullptr;
    QPushButton* updBtn_ = nullptr;
    QProgressBar* bar_;
    QLabel* barLbl_;
    QGridLayout* paletteGrid_ = nullptr;
    QGridLayout* allGrid_ = nullptr;
    QGridLayout* normalGrid_ = nullptr;
    QMap<QString, QString> latest_;
};
