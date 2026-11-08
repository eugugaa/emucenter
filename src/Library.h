#pragma once
#include <QDateTime>
#include <QList>
#include <QString>
#include <QStringList>
#include <functional>

struct Game {
    QString path, rawName, title, systemId, internalName;
    QString key() const;
};

namespace Library {
QList<Game> scan(const QStringList& dirs);
void sortGames(QList<Game>& games);
QString cleanTitle(const QString& raw);
QString resolveTitle(const Game& g);
QString detectSystemId(const QString& path);

bool importFile(const QString& src, const QString& destDir, bool move, const std::function<bool()>& canceled, QString* error);
qint64 playSeconds(const Game& g);
QDateTime lastPlayed(const Game& g);
void addPlayTime(const Game& g, qint64 secs);
QString formatDuration(qint64 secs);

struct GameOpts { QString emu; QString args; };
GameOpts options(const Game& g);
void setOptions(const Game& g, const GameOpts& o);
void resetPlayTime(const Game& g);
QString gameArgs(const Game& g);
void setGameArgs(const Game& g, const QString& a);
QString gameEmu(const Game& g);
void setGameEmu(const Game& g, const QString& v);
void resetPlay(const Game& g);
}
