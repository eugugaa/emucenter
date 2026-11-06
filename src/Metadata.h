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

// Metadados dos jogos: personalizado > IGDB > libretro-thumbnails/Wikipédia. Cache local + fila com progresso.
class Metadata : public QObject {
    Q_OBJECT
public:
    enum Kind { Icon = 0, Banner = 1 };
    Metadata(Igdb* igdb, QObject* parent = nullptr);

    void enqueueAll(const QList<Game>& games);
    void requestIcon(const Game& g);
    void requestBanner(const Game& g);               // imediato (jogo selecionado)
    void requestText(const Game& g);
    void loadCachedIcons(const QList<Game>& games);  // só do disco, sem rede
    void refreshGame(const Game& g);                 // apaga o cache automático e baixa de novo
    void resetGame(const Game& g);                   // apaga também tudo que foi personalizado
    void clearFailures();
    void clearCache();
    QString cacheDir() const;
    QString customDir() const;                       // imagens escolhidas pelo usuário
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

    // lista de imagens do libretro-thumbnails por console/pasta (para achar o jogo por nome aproximado)
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
