#pragma once
#include <QNetworkAccessManager>
#include <QObject>
#include <functional>

class Updater : public QObject {
    Q_OBJECT
public:
    explicit Updater(QObject* parent = nullptr);
    static QString currentVersion();
    static int compareVersions(const QString& a, const QString& b);
    void check(bool includePrerelease);
    void install();
    bool updateAvailable() const { return newer_; }
    QString latestTag() const { return tag_; }
    bool busy() const { return busy_; }
signals:
    void checked(bool ok, bool newer, const QString& tag, const QString& message);
    void progress(int percent, const QString& text);
    void failed(const QString& message);
private:
    QString base() const;
    QString workDir() const;
    void latestTag(bool includePre, std::function<void(QString, QString)> cb);
    void extractAndApply(const QString& zip);
    bool applyUpdate(const QString& srcDir);
    QNetworkAccessManager nam_;
    QString tag_;
    bool newer_ = false;
    bool busy_ = false;
};
