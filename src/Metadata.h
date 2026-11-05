#pragma once
#include "Igdb.h"
#include "Library.h"
#include <QHash>
#include <QNetworkAccessManager>
#include <QObject>
#include <QPixmap>
#include <QQueue>
#include <functional>

// Game metadata: custom > IGDB > libretro-thumbnails/Wikipedia. Local cache + queue with progress.
class Metadata : public QObject {
    Q_OBJECT
public:
    enum Kind { Icon = 0, Banner = 1 };
    Metadata(Igdb* igdb, QObject* parent = nullptr);

    void enqueueAll(const QList<Game>& games);
    void requestIcon(const Game& g);
    void requestBanner(const Game& g);                 // immediate (selected game)
    void requestText(const Game& g);
    void loadCachedIcons(const QList<Game>& games);  // disk only, no network
    void refreshGame(const Game& g);                 // clears automatic cache and downloads again
    void resetGame(const Game& g);                     // also clears everything customized by the user
    void clearFailures();
    void clearCache();
    QString cacheDir() const;
    QString customDir() const;                         // images chosen by the user
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
    QNetworkAccessManager nam_;
    Igdb* igdb_;
    QQueue<Task> queue_;
    QHash<QString, QList<std::function<void(const IgdbGame*)>>> pending_;
    int active_ = 0, total_ = 0, done_ = 0, downloaded_ = 0;
};