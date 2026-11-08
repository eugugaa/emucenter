#pragma once
#include "Library.h"
#include "Metadata.h"
#include <QButtonGroup>
#include <QSet>
#include <QWidget>
#include <functional>

class QLabel;
class QListWidget;
class QListWidgetItem;
class QPushButton;
class QHBoxLayout;
class QTimer;

class BigPicture : public QWidget {
    Q_OBJECT
public:
    explicit BigPicture(Metadata* meta, QWidget* parent = nullptr);
    void setGames(const QList<Game>& games, const std::function<bool(const Game&)>& playable);
    void focusGame(const QString& key);
    void refreshVisuals();
signals:
    void playRequested(const QString& key);
    void exitRequested();
    void fullscreenRequested();
protected:
    void paintEvent(QPaintEvent*) override;
    bool eventFilter(QObject*, QEvent*) override;
    void showEvent(QShowEvent*) override;
private:
    void rebuildChips();
    void rebuildList();
    void updateInfo();
    void stepChip(int delta);
    const Game* gameFor(const QString& key) const;

    Metadata* meta_;
    QList<Game> games_;
    QSet<QString> playable_;
    QString system_;
    QListWidget* list_;
    QLabel *title_, *meta1_, *desc_, *clock_;
    QPushButton *play_, *back_, *full_;
    QWidget* chipHost_;
    QHBoxLayout* chipLayout_;
    QButtonGroup chips_;
    QTimer* clockTimer_;
};
