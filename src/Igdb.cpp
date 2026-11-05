#include "Igdb.h"
#include "I18n.h"
#include <QDateTime>
#include <QJsonArray>
#include <QJsonDocument>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QSettings>
#include <QUrl>
#include <QUrlQuery>

static const char* kFields = "fields name,summary,first_release_date,cover.image_id,artworks.image_id,screenshots.image_id;";

static qint64 platformId(const QString& sys) {   // ids de plataforma do IGDB
    static const QMap<QString, qint64> m = {{"nes", 18}, {"snes", 19}, {"n64", 4}, {"gc", 21}, {"wii", 5}, {"wiiu", 41},
                                            {"gb", 33}, {"gbc", 22}, {"gba", 24}, {"nds", 20}, {"3ds", 37}, {"ps1", 7},
                                            {"ps2", 8}, {"psp", 38}, {"ps3", 9}, {"genesis", 29}};
    return m.value(sys, 0);
}

Igdb::Igdb(QObject* p) : QObject(p) {}

bool Igdb::configured() const {
    QSettings s;
    return !s.value("igdb/clientId").toString().trimmed().isEmpty() && !s.value("igdb/clientSecret").toString().trimmed().isEmpty();
}

QUrl Igdb::imageUrl(const QString& id, const QString& size) {
    return QUrl("https://images.igdb.com/igdb/image/upload/t_" + size + "/" + id + ".jpg");
}

void Igdb::getImage(const QUrl& url, std::function<void(QPixmap)> cb) {
    QNetworkRequest rq(url);
    rq.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::NoLessSafeRedirectPolicy);
    QNetworkReply* r = nam_.get(rq);
    connect(r, &QNetworkReply::finished, this, [r, cb]() {
        r->deleteLater();
        QPixmap pm;
        if (r->error() == QNetworkReply::NoError) pm.loadFromData(r->readAll());
        cb(pm);
    });
}

void Igdb::withToken(std::function<void(bool, QString)> cb) {
    QSettings s;
    const qint64 now = QDateTime::currentSecsSinceEpoch();
    if (!s.value("igdb/token").toString().isEmpty() && s.value("igdb/tokenExp").toLongLong() > now + 60)
        return cb(true, s.value("igdb/token").toString());

    QUrl u("https://id.twitch.tv/oauth2/token");
    QUrlQuery q;
    q.addQueryItem("client_id", s.value("igdb/clientId").toString().trimmed());
    q.addQueryItem("client_secret", s.value("igdb/clientSecret").toString().trimmed());
    q.addQueryItem("grant_type", "client_credentials");
    u.setQuery(q);
    QNetworkReply* r = nam_.post(QNetworkRequest(u), QByteArray());
    connect(r, &QNetworkReply::finished, this, [r, cb]() {
        r->deleteLater();
        const auto o = QJsonDocument::fromJson(r->readAll()).object();
        const QString tok = o["access_token"].toString();
        if (tok.isEmpty()) return cb(false, o["message"].toString().isEmpty() ? r->errorString() : o["message"].toString());
        QSettings st;
        st.setValue("igdb/token", tok);
        st.setValue("igdb/tokenExp", QDateTime::currentSecsSinceEpoch() + o["expires_in"].toInteger());
        cb(true, tok);
    });
}

void Igdb::query(const QByteArray& body, ListCb cb, bool retry) {
    withToken([this, body, cb, retry](bool ok, QString tok) {
        if (!ok) return cb({}, tok);
        QNetworkRequest rq(QUrl("https://api.igdb.com/v4/games"));
        rq.setRawHeader("Client-ID", QSettings().value("igdb/clientId").toString().trimmed().toUtf8());
        rq.setRawHeader("Authorization", "Bearer " + tok.toUtf8());
        rq.setRawHeader("Accept", "application/json");
        rq.setHeader(QNetworkRequest::ContentTypeHeader, "text/plain");
        QNetworkReply* r = nam_.post(rq, body);
        connect(r, &QNetworkReply::finished, this, [this, r, body, cb, retry]() {
            r->deleteLater();
            const int code = r->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
            if (code == 401 && retry) {                 // token expirado/inválido: renova e tenta uma vez
                QSettings().remove("igdb/token");
                return query(body, cb, false);
            }
            const auto doc = QJsonDocument::fromJson(r->readAll());
            if (code != 200 || !doc.isArray()) return cb({}, T("IGDB respondeu com erro (HTTP %1).").arg(code));
            QList<IgdbGame> out;
            for (const auto& v : doc.array()) out << fromJson(v.toObject());
            cb(out, QString());
        });
    });
}

IgdbGame Igdb::fromJson(const QJsonObject& o) {
    IgdbGame g;
    g.id = o["id"].toInteger();
    g.name = o["name"].toString();
    g.summary = o["summary"].toString();
    if (o.contains("first_release_date")) g.year = QDateTime::fromSecsSinceEpoch(o["first_release_date"].toInteger()).date().year();
    if (o.contains("year")) g.year = o["year"].toInt();
    // aceita o formato da API (objetos com image_id) e o formato salvo no cache (strings)
    auto idOf = [](const QJsonValue& v) { return v.isObject() ? v.toObject()["image_id"].toString() : v.toString(); };
    g.cover = idOf(o["cover"]);
    for (const auto& a : o["artworks"].toArray()) g.artworks << idOf(a);
    for (const auto& s : o["screenshots"].toArray()) g.screenshots << idOf(s);
    g.artworks.removeAll({});
    g.screenshots.removeAll({});
    return g;
}

QJsonObject Igdb::toJson(const IgdbGame& g) {
    return {{"id", g.id}, {"name", g.name}, {"summary", g.summary}, {"year", g.year}, {"cover", g.cover},
            {"artworks", QJsonArray::fromStringList(g.artworks)}, {"screenshots", QJsonArray::fromStringList(g.screenshots)}};
}

void Igdb::search(const QString& name, const QString& sys, ListCb cb) {
    QString n = name;
    n.replace('\\', ' ').replace('"', ' ');
    const qint64 pid = platformId(sys);
    auto run = [this, n, cb](qint64 platform) {
        QString body = "search \"" + n + "\"; " + kFields + " ";
        if (platform) body += "where platforms = (" + QString::number(platform) + "); ";
        body += "limit 8;";
        query(body.toUtf8(), cb);
    };
    if (!pid) return run(0);
    query(("search \"" + n + "\"; " + kFields + " where platforms = (" + QString::number(pid) + "); limit 8;").toUtf8(),
          [run, cb](QList<IgdbGame> l, QString err) {
              if (!l.isEmpty() || !err.isEmpty()) return cb(l, err);
              run(0);                                    // sem resultado na plataforma: tenta sem filtro
          });
}

void Igdb::byId(qint64 id, ListCb cb) {
    query((QString(kFields) + " where id = " + QString::number(id) + "; limit 1;").toUtf8(), cb);
}

void Igdb::test(std::function<void(bool, QString)> cb) {
    QSettings().remove("igdb/token");
    withToken([cb](bool ok, QString m) { cb(ok, ok ? T("Conectado ao IGDB.") : m); });
}
