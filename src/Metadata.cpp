#include "Metadata.h"
#include "Catalog.h"
#include "I18n.h"
#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkReply>
#include <QSettings>
#include <QStandardPaths>
#include <QTimer>

Metadata::Metadata(Igdb* igdb, QObject* p) : QObject(p), igdb_(igdb) {
    QDir().mkpath(cacheDir());
    QDir().mkpath(customDir());
}

QString Metadata::cacheDir() const { return QStandardPaths::writableLocation(QStandardPaths::CacheLocation); }
QString Metadata::customDir() const { return QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) + "/custom"; }

qint64 Metadata::cacheBytes() const {
    qint64 n = 0;
    QDirIterator it(cacheDir(), QDir::Files);
    while (it.hasNext()) { it.next(); n += it.fileInfo().size(); }
    return n;
}
void Metadata::clearCache() {
    QDirIterator it(cacheDir(), QDir::Files);
    while (it.hasNext()) QFile::remove(it.next());
}
void Metadata::clearFailures() {
    QDirIterator it(cacheDir(), {"*.none"}, QDir::Files);
    while (it.hasNext()) QFile::remove(it.next());
}
void Metadata::refreshGame(const Game& g) {
    QDirIterator it(cacheDir(), {g.key() + "*"}, QDir::Files);
    while (it.hasNext()) QFile::remove(it.next());
}
void Metadata::resetGame(const Game& g) {
    refreshGame(g);
    QDirIterator it(customDir(), {g.key() + "*"}, QDir::Files);
    while (it.hasNext()) QFile::remove(it.next());
    QSettings s;
    s.remove("game/" + g.key() + "/name");
    s.remove("game/" + g.key() + "/desc");
    s.remove("game/" + g.key() + "/igdbId");
    s.remove("igdb/" + g.key());
}

static QString sanitize(QString n) {   // libretro-thumbnails rule
    for (QChar c : QString("&*/:`<>?\\|\"")) n.replace(c, '_');
    return n;
}
static QString norm(const QString& s) {
    QString o;
    for (QChar c : s.toLower()) if (c.isLetterOrNumber()) o += c;
    return o;
}

// ---------- queue ----------
void Metadata::addTask(Task t) {
    queue_.enqueue(std::move(t));
    ++total_;
    emit progress(done_, total_);
    pump();
}

void Metadata::pump() {
    const int maxActive = qBound(1, QSettings().value("net/parallel", 4).toInt(), 8);
    while (active_ < maxActive && !queue_.isEmpty()) {
        ++active_;
        Task t = queue_.dequeue();
        t([this]() {
            --active_; ++done_;
            emit progress(done_, total_);
            if (active_ == 0 && queue_.isEmpty()) {
                const int tot = total_, dl = downloaded_;
                total_ = done_ = downloaded_ = 0;
                emit batchFinished(tot, dl);
            } else pump();
        });
    }
}

void Metadata::enqueueAll(const QList<Game>& games) {
    for (const Game& g : games) {
        addTask([this, g](Cb d) { fetchImage(g, Icon, d); });
        addTask([this, g](Cb d) { fetchImage(g, Banner, d); });
        addTask([this, g](Cb d) { fetchText(g, d); });
    }
}
void Metadata::requestIcon(const Game& g) { addTask([this, g](Cb d) { fetchImage(g, Icon, d); }); }
void Metadata::requestBanner(const Game& g) { fetchImage(g, Banner, [] {}); }
void Metadata::requestText(const Game& g) { fetchText(g, [] {}); }

void Metadata::loadCachedIcons(const QList<Game>& games) {
    for (const Game& g : games) {
        QString p = customDir() + "/" + g.key() + "_icon.png";
        if (!QFile::exists(p)) p = cacheDir() + "/" + g.key() + "_icon.png";
        if (QFile::exists(p)) emit imageReady(g.key(), Icon, QPixmap(p));
    }
}

// ---------- IGDB (cached result per game) ----------
void Metadata::withIgdb(const Game& g, std::function<void(const IgdbGame*)> cb) {
    QSettings s;
    if (!s.value("useIgdb", true).toBool() || !igdb_->configured()) return cb(nullptr);
    const QString key = g.key();
    const QString f = cacheDir() + "/" + key + "_igdb.json";
    if (QFile::exists(f)) {
        QFile jf(f); jf.open(QIODevice::ReadOnly);
        const IgdbGame r = Igdb::fromJson(QJsonDocument::fromJson(jf.readAll()).object());
        return cb(&r);
    }
    if (QFile::exists(f + ".none")) return cb(nullptr);
    if (pending_.contains(key)) { pending_[key].append(cb); return; }   // another task is already fetching it
    pending_[key] = {cb};

    auto finish = [this, key, f](bool ok, IgdbGame r, bool mark) {
        if (ok) {
            QFile jf(f);
            if (jf.open(QIODevice::WriteOnly)) jf.write(QJsonDocument(Igdb::toJson(r)).toJson(QJsonDocument::Compact));
            QSettings st;
            st.setValue("igdb/" + key + "/name", r.name);
            ++downloaded_;
            if (st.value("useIgdbName", true).toBool() && st.value("game/" + key + "/name").toString().isEmpty()) emit nameReady(key, r.name);
        } else if (mark) {
            QFile jf(f + ".none"); jf.open(QIODevice::WriteOnly);
        }
        const auto cbs = pending_.take(key);
        for (const auto& c : cbs) c(ok ? &r : nullptr);
    };

    const qint64 pick = s.value("game/" + key + "/igdbId", 0).toLongLong();
    if (pick) {   // the user manually chose which IGDB game this is
        igdb_->byId(pick, [finish](QList<IgdbGame> l, QString err) {
            if (!l.isEmpty()) finish(true, l.first(), false);
            else finish(false, {}, err.isEmpty());
        });
        return;
    }
    QStringList names;
    if (!g.internalName.isEmpty()) names << g.internalName;
    const QString fn = Library::cleanTitle(g.rawName);
    if (!names.contains(fn)) names << fn;
    const QString sys = g.systemId;

    auto attempt = std::make_shared<std::function<void(int)>>();
    *attempt = [this, names, sys, finish, attempt](int i) {
        if (i >= names.size()) return finish(false, {}, true);
        igdb_->search(names[i], sys, [=](QList<IgdbGame> l, QString err) {
            if (!err.isEmpty()) return finish(false, {}, false);   // network/credential error: do not mark as "not found"
            if (l.isEmpty()) return (*attempt)(i + 1);
            IgdbGame best = l.first();
            for (const auto& c : l) if (norm(c.name) == norm(names[i])) { best = c; break; }
            finish(true, best, false);
        });
    };
    (*attempt)(0);
}

// ---------- images ----------
void Metadata::fetchImage(const Game& g, int kind, Cb done) {
    const QString suffix = kind == Banner ? "_banner.png" : "_icon.png";
    auto later = [done] { QTimer::singleShot(0, done); };   // prevents deep recursion when everything is cached
    const QString custom = customDir() + "/" + g.key() + suffix;
    if (QFile::exists(custom)) { emit imageReady(g.key(), kind, QPixmap(custom)); return later(); }
    const QString cache = cacheDir() + "/" + g.key() + suffix;
    if (QFile::exists(cache)) { emit imageReady(g.key(), kind, QPixmap(cache)); return later(); }
    if (QFile::exists(cache + ".none")) return later();

    withIgdb(g, [this, g, kind, cache, done](const IgdbGame* r) {
        QList<QUrl> urls;
        if (r) {
            if (kind == Icon && !r->cover.isEmpty()) urls << Igdb::imageUrl(r->cover, "cover_big");
            if (kind == Banner) {
                if (!r->artworks.isEmpty()) urls << Igdb::imageUrl(r->artworks.first(), "1080p");
                if (!r->screenshots.isEmpty()) urls << Igdb::imageUrl(r->screenshots.first(), "1080p");
            }
        }
        if (const SystemDef* s = Catalog::system(g.systemId)) {   // alternative: libretro-thumbnails
            const QStringList dirs = kind == Banner ? QStringList{"Named_Titles", "Named_Snaps"} : QStringList{"Named_Boxarts", "Named_Titles"};
            QStringList names = {sanitize(g.rawName)};
            if (sanitize(g.title) != names[0]) names << sanitize(g.title);
            for (const QString& d : dirs)
                for (const QString& n : names)
                    urls << QUrl::fromEncoded("https://thumbnails.libretro.com/" + QUrl::toPercentEncoding(s->thumbDir) + "/" + d.toUtf8() +
                                             "/" + QUrl::toPercentEncoding(n) + ".png");
        }
        tryUrls(g, kind, urls, cache, done);
    });
}

void Metadata::tryUrls(const Game& g, int kind, QList<QUrl> urls, const QString& cache, Cb done) {
    if (urls.isEmpty()) {
        QFile f(cache + ".none"); f.open(QIODevice::WriteOnly);   // do not try again (until "retry failures")
        return done();
    }
    QNetworkRequest rq(urls.takeFirst());
    rq.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::NoLessSafeRedirectPolicy);
    QNetworkReply* r = nam_.get(rq);
    connect(r, &QNetworkReply::finished, this, [=]() mutable {
        r->deleteLater();
        QPixmap pm;
        if (r->error() == QNetworkReply::NoError && pm.loadFromData(r->readAll())) {
            pm.save(cache, "PNG");
            ++downloaded_;
            emit imageReady(g.key(), kind, pm);
            return done();
        }
        tryUrls(g, kind, urls, cache, done);
    });
}

// ---------- description: custom > IGDB > Wikipedia ----------
void Metadata::fetchText(const Game& g, Cb done) {
    auto later = [done] { QTimer::singleShot(0, done); };
    const QString custom = QSettings().value("game/" + g.key() + "/desc").toString();
    if (!custom.isEmpty()) { emit textReady(g.key(), custom); return later(); }

    const QString cache = cacheDir() + "/" + g.key() + "_" + I18n::lang() + ".txt";
    if (QFile::exists(cache)) {
        QFile f(cache); f.open(QIODevice::ReadOnly);
        emit textReady(g.key(), QString::fromUtf8(f.readAll()));
        return later();
    }
    if (QFile::exists(cache + ".none")) return later();

    withIgdb(g, [this, g, cache, done](const IgdbGame* r) {
        if (r && !r->summary.isEmpty()) {
            const QString txt = r->summary + "\n\n(" + T("Source: IGDB") + ")";
            QFile f(cache); f.open(QIODevice::WriteOnly); f.write(txt.toUtf8());
            ++downloaded_;
            emit textReady(g.key(), txt);
            return done();
        }
        fetchWikipedia(g, cache, done);
    });
}

void Metadata::fetchWikipedia(const Game& g, const QString& cache, Cb done) {
    QList<QPair<QString, QString>> tries;   // (wiki language, title)
    QStringList langs = {I18n::lang()};
    if (langs[0] != "en") langs << "en";
    for (const QString& l : langs) {
        tries << qMakePair(l, g.title + (l == "pt" ? " (jogo eletrônico)" : " (video game)"));
        tries << qMakePair(l, g.title);
    }
    auto attempt = std::make_shared<std::function<void(int)>>();
    *attempt = [=](int i) {
        if (i >= tries.size()) {
            QFile f(cache + ".none"); f.open(QIODevice::WriteOnly);
            return done();
        }
        const QString t = QString(tries[i].second).replace(' ', '_');
        QNetworkRequest rq(QUrl::fromEncoded("https://" + tries[i].first.toUtf8() + ".wikipedia.org/api/rest_v1/page/summary/" +
                                             QUrl::toPercentEncoding(t)));
        rq.setHeader(QNetworkRequest::UserAgentHeader, "EmuCenter/0.3");
        QNetworkReply* r = nam_.get(rq);
        connect(r, &QNetworkReply::finished, this, [=]() {
            r->deleteLater();
            const auto o = QJsonDocument::fromJson(r->readAll()).object();
            const QString ex = o["extract"].toString();
            if (r->error() == QNetworkReply::NoError && !ex.isEmpty() && o["type"].toString() != "disambiguation") {
                const QString txt = ex + "\n\n(" + T("Source: Wikipedia") + ")";
                QFile f(cache); f.open(QIODevice::WriteOnly); f.write(txt.toUtf8());
                ++downloaded_;
                emit textReady(g.key(), txt);
                done();
            } else (*attempt)(i + 1);
        });
    };
    (*attempt)(0);
}