#pragma once
#include "Igdb.h"
#include "Library.h"
#include <QHash>
#include <QNetworkAccessManager>
#include <QObject>
#include <QPixmap>
#include <QQueue>
#include <functional>
#include <memory>

class Metadata : public QObject {
    Q_OBJECT
public:
    enum Kind { Icon = 0, Banner = 1 };
    Metadata(Igdb* igdb, QObject* parent = nullptr);

    void enqueueAll(const QList<Game>& games);
    void requestIcon(const Game& g);
    void requestBanner(const Game& g);
    void requestText(const Game& g);
    void loadCachedIcons(const QList<Game>& games);
    void refreshGame(const Game& g);
    void resetGame(const Game& g);
    void clearFailures();
    void clearCache();
    QString cacheDir() const;
    QString customDir() const;
    QString iconPath(const QString& key) const;
    QString bannerPath(const QString& key) const;
    QString cachedText(const Game& g) const;
    qint64 cacheBytes() const;

signals:
    void imageReady(const QString& key, int kind, const QPixmap& pm);
    void textReady(const QString& key, const QString& text);
    void nameReady(const QString& key, const QString& name);
    void progress(int done, int total);
    void batchFinished(int total, int downloaded);

private:
    using Cb = std::function<void()>;
    using Task = std::function<void(Cb)>;
    void addTask(Task t);
    void pump();
    void withIgdb(const Game& g, std::function<void(const IgdbGame*)> cb);
    void fetchImage(const Game& g, int kind, Cb done);
    void fetchText(const Game& g, Cb done);
    void fetchWikipedia(const Game& g, const QString& cache, Cb done);
    void tryUrls(const Game& g, int kind, QList<QUrl> urls, const QString& cache, Cb done);

    struct Listing { QStringList names, norms; };
    void loadListing(const QString& thumbDir, const QString& dir, std::function<void(std::shared_ptr<Listing>)> cb);
    void libretroUrls(const Game& g, int kind, std::function<void(QList<QUrl>)> cb);
    QHash<QString, std::shared_ptr<Listing>> listings_;
    QHash<QString, QList<std::function<void(std::shared_ptr<Listing>)>>> listPending_;
    QNetworkAccessManager nam_;
    Igdb* igdb_;
    QQueue<Task> queue_;
    QHash<QString, QList<std::function<void(const IgdbGame*)>>> pending_;
    int active_ = 0, total_ = 0, done_ = 0, downloaded_ = 0;
};
