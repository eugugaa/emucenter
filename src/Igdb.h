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
    QStringList artworks, screenshots;
};

class Igdb : public QObject {
    Q_OBJECT
public:
    using ListCb = std::function<void(QList<IgdbGame>, QString err)>;
    explicit Igdb(QObject* parent = nullptr);
    bool configured() const;
    QString clientId() const;
    QString clientSecret() const;
    void search(const QString& name, const QString& systemId, ListCb cb);
    void byId(qint64 id, ListCb cb);
    void test(std::function<void(bool, QString)> cb);
    void getImage(const QUrl& url, std::function<void(QPixmap)> cb);
    static QUrl imageUrl(const QString& imageId, const QString& size);
    static QJsonObject toJson(const IgdbGame& g);
    static IgdbGame fromJson(const QJsonObject& o);
private:
    void withToken(std::function<void(bool ok, QString tokenOrErr)> cb);
    void query(const QByteArray& body, ListCb cb, bool retry = true);
    QNetworkAccessManager nam_;
};
