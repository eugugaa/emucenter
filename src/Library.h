#pragma once
#include <QDateTime>
#include <QList>
#include <QString>
#include <QStringList>

struct Game {
    QString path, rawName, title, systemId, internalName;
    QString key() const;   // stable ID (path hash)
};

namespace Library {
QList<Game> scan(const QStringList& dirs);
QString cleanTitle(const QString& raw);
QString resolveTitle(const Game& g);   // custom > IGDB > ROM internal name > filename
qint64 playSeconds(const Game& g);
QDateTime lastPlayed(const Game& g);
void addPlayTime(const Game& g, qint64 secs);
QString formatDuration(qint64 secs);

struct GameOpts { QString emu; QString args; };   // emu = "emuId|core" ("" = automatic)
GameOpts options(const Game& g);
void setOptions(const Game& g, const GameOpts& o);
void resetPlayTime(const Game& g);
QString gameArgs(const Game& g);                 // extra arguments per game
void setGameArgs(const Game& g, const QString& a);
QString gameEmu(const Game& g);                  // "emuId|core" or "" (automatic)
void setGameEmu(const Game& g, const QString& v);
void resetPlay(const Game& g);
}