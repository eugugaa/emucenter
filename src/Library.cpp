#include "Library.h"
#include "Catalog.h"
#include "RomInfo.h"
#include <QCryptographicHash>
#include <QDirIterator>
#include <QFile>
#include <QFileInfo>
#include <QRegularExpression>
#include <QSettings>

QString Game::key() const {
    return QString::fromLatin1(QCryptographicHash::hash(path.toUtf8(), QCryptographicHash::Sha1).toHex());
}

// GameCube/Wii ISOs have a "magic signature" in the header
static QString headerSystem(const QString& path) {
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly)) return {};
    const QByteArray h = f.read(0x20);
    if (h.size() < 0x20) return {};
    auto b = [&](int i) { return static_cast<uchar>(h[i]); };
    if (b(0x1C) == 0xC2 && b(0x1D) == 0x33 && b(0x1E) == 0x9F && b(0x1F) == 0x3D) return "gc";
    if (b(0x18) == 0x5D && b(0x19) == 0x1C && b(0x1A) == 0x9E && b(0x1B) == 0xA3) return "wii";
    return {};
}

static const SystemDef* detect(const QString& path) {
    const QString ext = QFileInfo(path).suffix().toLower();
    QList<const SystemDef*> cand;
    for (const auto& s : Catalog::systems())
        if (s.exts.contains(ext)) cand << &s;
    if (cand.isEmpty()) return nullptr;
    if (cand.size() == 1) return cand.first();

    if (ext == "iso" || ext == "gcm") {
        const QString h = headerSystem(path);
        for (auto* s : cand) if (s->id == h) return s;
    }
    // ambiguous extension: uses folder names ("PS2", "GameCube"...) as a hint
    const QStringList parts = QFileInfo(path).absolutePath().toLower().split('/');
    const SystemDef* best = nullptr;
    int bestScore = 0;
    for (auto* s : cand)
        for (const QString& hint : s->folderHints)
            for (const QString& p : parts) {
                const bool hit = hint.size() <= 3 ? p == hint : p.contains(hint);
                if (hit && hint.size() > bestScore) { bestScore = hint.size(); best = s; }
            }
    return best ? best : cand.first();
}

QString Library::cleanTitle(const QString& raw) {
    QString t = raw;
    t.remove(QRegularExpression(R"(\s*[\(\[][^\)\]]*[\)\]])"));
    t.replace('_', ' ');
    return t.simplified();
}

QString Library::resolveTitle(const Game& g) {
    QSettings s;
    const QString k = g.key();
    const QString custom = s.value("game/" + k + "/name").toString();
    if (!custom.isEmpty()) return custom;
    if (s.value("useIgdbName", true).toBool()) {
        const QString n = s.value("igdb/" + k + "/name").toString();
        if (!n.isEmpty()) return n;
    }
    if (s.value("useInternalName", true).toBool() && !g.internalName.isEmpty()) return g.internalName;
    return cleanTitle(g.rawName);
}

QList<Game> Library::scan(const QStringList& dirs) {
    QList<Game> out;
    for (const QString& d : dirs) {
        QDirIterator it(d, QDir::Files, QDirIterator::Subdirectories);
        while (it.hasNext()) {
            const QString p = it.next();
            if (const SystemDef* s = detect(p)) {
                Game g;
                g.path = p;
                g.rawName = QFileInfo(p).completeBaseName();
                g.systemId = s->id;
                g.internalName = RomInfo::internalName(p, s->id);
                g.title = resolveTitle(g);
                out << g;
            }
        }
    }
    std::sort(out.begin(), out.end(), [](const Game& a, const Game& b) {
        return a.title.localeAwareCompare(b.title) < 0;
    });
    return out;
}

qint64 Library::playSeconds(const Game& g) {
    return QSettings().value("play/" + g.key() + "/secs", 0).toLongLong();
}
QDateTime Library::lastPlayed(const Game& g) {
    return QSettings().value("play/" + g.key() + "/last").toDateTime();
}
void Library::addPlayTime(const Game& g, qint64 secs) {
    QSettings s;
    s.setValue("play/" + g.key() + "/secs", playSeconds(g) + secs);
    s.setValue("play/" + g.key() + "/last", QDateTime::currentDateTime());
}
QString Library::formatDuration(qint64 s) {
    if (s < 60) return s == 0 ? "—" : QString("%1 s").arg(s);
    const qint64 m = s / 60;
    return m < 60 ? QString("%1 min").arg(m) : QString("%1 h %2 min").arg(m / 60).arg(m % 60);
}

QString Library::gameArgs(const Game& g) { return QSettings().value("game/" + g.key() + "/args").toString(); }
void Library::setGameArgs(const Game& g, const QString& a) { QSettings().setValue("game/" + g.key() + "/args", a); }
QString Library::gameEmu(const Game& g) { return QSettings().value("game/" + g.key() + "/emu").toString(); }
void Library::setGameEmu(const Game& g, const QString& v) { QSettings().setValue("game/" + g.key() + "/emu", v); }
void Library::resetPlay(const Game& g) {
    QSettings s;
    s.remove("play/" + g.key() + "/secs");
    s.remove("play/" + g.key() + "/last");
}

Library::GameOpts Library::options(const Game& g) {
    QSettings s;
    return {s.value("game/" + g.key() + "/emu").toString(), s.value("game/" + g.key() + "/args").toString()};
}
void Library::setOptions(const Game& g, const GameOpts& o) {
    QSettings s;
    s.setValue("game/" + g.key() + "/emu", o.emu);
    s.setValue("game/" + g.key() + "/args", o.args);
}
void Library::resetPlayTime(const Game& g) {
    QSettings s;
    s.remove("play/" + g.key() + "/secs");
    s.remove("play/" + g.key() + "/last");
}