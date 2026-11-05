#pragma once
#include "Emulators.h"
#include "Igdb.h"
#include "Library.h"
#include "Metadata.h"
#include <QDialog>

class QComboBox;
class QLabel;
class QLineEdit;
class QListWidget;
class QPushButton;
class QTextEdit;

// Configurações de um jogo: nome, descrição, ícone, banner (com busca no IGDB) e execução.
class GameSettingsDialog : public QDialog {
    Q_OBJECT
public:
    GameSettingsDialog(const Game& g, EmulatorManager* emu, Metadata* meta, Igdb* igdb, QWidget* parent);
    bool changed() const { return changed_; }
private slots:
    void searchIgdb();
    void pickResult();
    void save();
private:
    void showIcon(const QPixmap& pm);
    void showBanner(const QPixmap& pm);
    void fillOptions(const IgdbGame& g);
    QWidget* buildInfo();
    QWidget* buildImages();
    QWidget* buildRun();

    Game game_;
    EmulatorManager* emu_;
    Metadata* meta_;
    Igdb* igdb_;
    QList<IgdbGame> results_;
    qint64 pickedId_ = 0;
    QLineEdit *name_, *args_;
    QTextEdit* desc_;
    QString origDesc_;
    QListWidget *resultList_, *iconOpts_, *bannerOpts_;
    QLabel *iconPrev_, *bannerPrev_, *status_;
    QPushButton* searchBtn_;
    QComboBox* emuBox_;
    QPixmap pendIcon_, pendBanner_;
    bool resetIcon_ = false, resetBanner_ = false, changed_ = false;
};
