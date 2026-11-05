#pragma once
#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QObject>
#include <QPixmap>
#include <QStringList>
#include <functional>

struct IgdbGame {
    qint64 id = 0;
    QString name, summary, cover;
    int year = 0;
    QStringList artworks, screenshots;     // image_id
};

// IGDB (Twitch) API client. Requires its own Client ID and Client Secret (free at dev.twitch.tv).
class Igdb : public QObject {
    Q_OBJECT
public:
    using ListCb = std::function<void(QList<IgdbGame>, QString err)>;
    explicit Igdb(QObject* parent = nullptr);
    bool configured() const;
    void search(const QString& name, const QString& systemId, ListCb cb);
    void byId(qint64 id, ListCb cb);
    void test(std::function<void(bool, QString)> cb);
    void getImage(const QUrl& url, std::function<void(QPixmap)> cb);
    static QUrl imageUrl(const QString& imageId, const QString& size);   // cover_big, 1080p, thumb...
    static QJsonObject toJson(const IgdbGame& g);
    static IgdbGame fromJson(const QJsonObject& o);
private:
    void withToken(std::function<void(bool ok, QString tokenOrErr)> cb);
    void query(const QByteArray& body, ListCb cb, bool retry = true);
    QNetworkAccessManager nam_;
};