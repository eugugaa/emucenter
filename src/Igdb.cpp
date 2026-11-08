#include "Igdb.h"
#include "Config.h"
#include "I18n.h"
#include "Catalog.h"
#include <QDateTime>
#include <QJsonArray>
#include <QJsonDocument>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QSettings>
#include <QUrl>
#include <QUrlQuery>

static const char* kFields = "fields name,summary,first_release_date,cover.image_id,artworks.image_id,screenshots.image_id;";

static qint64 platformId(const QString& sys) {
    const SystemDef* d = Catalog::system(sys);
    return d ? d->igdbId : 0;
}

Igdb::Igdb(QObject* p) : QObject(p) {}

QString Igdb::clientId() const {
    const QString o = QSettings().value("igdb/clientId").toString().trimmed();
    return o.isEmpty() ? QString::fromUtf8(Config::IgdbClientId) : o;
}
QString Igdb::clientSecret() const {
    const QString o = QSettings().value("igdb/clientSecret").toString().trimmed();
    return o.isEmpty() ? QString::fromUtf8(Config::IgdbClientSecret) : o;
}
bool Igdb::configured() const { return !clientId().isEmpty() && !clientSecret().isEmpty(); }

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
    q.addQueryItem("client_id", clientId());
    q.addQueryItem("client_secret", clientSecret());
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
        rq.setRawHeader("Client-ID", clientId().toUtf8());
        rq.setRawHeader("Authorization", "Bearer " + tok.toUtf8());
        rq.setRawHeader("Accept", "application/json");
        rq.setHeader(QNetworkRequest::ContentTypeHeader, "text/plain");
        QNetworkReply* r = nam_.post(rq, body);
        connect(r, &QNetworkReply::finished, this, [this, r, body, cb, retry]() {
            r->deleteLater();
            const int code = r->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
            if (code == 401 && retry) {
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

    auto idOf = [](const QJsonValue& v) {
        if (!v.isObject()) return v.toString();
        const QJsonObject vo = v.toObject();
        return vo["image_id"].toString();
    };
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
              run(0);
          });
}

void Igdb::byId(qint64 id, ListCb cb) {
    query((QString(kFields) + " where id = " + QString::number(id) + "; limit 1;").toUtf8(), cb);
}

void Igdb::test(std::function<void(bool, QString)> cb) {
    QSettings().remove("igdb/token");
    withToken([cb](bool ok, QString m) { cb(ok, ok ? T("Conectado ao IGDB.") : m); });
}
