#pragma once
#include <QDateTime>
#include <QList>
#include <QString>
#include <QStringList>
#include <functional>

struct Game {
    QString path, rawName, title, systemId, internalName;
    QString key() const;   // id estável (hash do caminho)
};

namespace Library {
QList<Game> scan(const QStringList& dirs);
QString cleanTitle(const QString& raw);
QString resolveTitle(const Game& g);
QString detectSystemId(const QString& path);   // "" se a extensão não for de nenhum console
// Copia (ou move) uma ROM para destDir/<Console>/. Arquivos .cue levam junto os .bin que citam.
// canceled() é chamado durante a cópia; retorne true para abortar.
bool importFile(const QString& src, const QString& destDir, bool move, const std::function<bool()>& canceled, QString* error);   // personalizado > IGDB > nome interno da ROM > nome do arquivo
qint64 playSeconds(const Game& g);
QDateTime lastPlayed(const Game& g);
void addPlayTime(const Game& g, qint64 secs);
QString formatDuration(qint64 secs);

struct GameOpts { QString emu; QString args; };   // emu = "emuId|core" ("" = automático)
GameOpts options(const Game& g);
void setOptions(const Game& g, const GameOpts& o);
void resetPlayTime(const Game& g);
QString gameArgs(const Game& g);                 // argumentos extras por jogo
void setGameArgs(const Game& g, const QString& a);
QString gameEmu(const Game& g);                  // "emuId|core" ou "" (automático)
void setGameEmu(const Game& g, const QString& v);
void resetPlay(const Game& g);
}
