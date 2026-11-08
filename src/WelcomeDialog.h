#pragma once
#include "Emulators.h"
#include "Library.h"
#include <QDialog>
#include <QList>
#include <QPair>

class QLabel;
class QProgressBar;
class QPushButton;
class QStackedWidget;
class QTableWidget;
class QListWidget;
class QCheckBox;

class WelcomeDialog : public QDialog {
    Q_OBJECT
public:
    explicit WelcomeDialog(EmulatorManager* emu, QWidget* parent = nullptr);
    bool languageChanged() const { return langChanged_; }
private:
    void build();
    void go(int page);
    QWidget* pageLanguage();
    QWidget* pageRoms();
    QWidget* pageEmus();
    QWidget* pageOptions();
    QWidget* pageDone();
    void refreshRoms();
    void refreshEmus();
    void installSelected();
    void installNext();

    struct Row {
        QString emuId;
        QStringList cores;
        QStringList consoles;
        QCheckBox* check = nullptr;
    };

    EmulatorManager* emu_;
    QWidget* root_ = nullptr;
    QStackedWidget* stack_ = nullptr;
    QPushButton *back_ = nullptr, *next_ = nullptr, *skip_ = nullptr;
    QListWidget* romList_ = nullptr;
    QLabel* romInfo_ = nullptr;
    QTableWidget* emuTable_ = nullptr;
    QLabel* emuStatus_ = nullptr;
    QProgressBar* bar_ = nullptr;
    QLabel* summary_ = nullptr;
    QList<Row> rows_;
    QList<QPair<QString, QString>> queue_;
    QList<Game> found_;
    int page_ = 0;
    bool langChanged_ = false;
    bool installing_ = false;
};
