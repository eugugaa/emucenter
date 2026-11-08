#pragma once
#include "Catalog.h"
#include <QNetworkAccessManager>
#include <QObject>
#include <functional>

class EmulatorManager : public QObject {
    Q_OBJECT
public:
    explicit EmulatorManager(QObject* parent = nullptr);

    QString root() const;
    QString emuDir(const QString& id) const;
    void setEmuDir(const QString& id, const QString& dir);
    void setRoot(const QString& dir);
    QString findExe(const QString& id) const;
    QString corePath(const QString& core) const;
    bool available(const EmuLink& l) const;
    bool buildCommand(const EmuLink& l, const QString& rom, QString& prog, QStringList& args) const;
    QString installedVersion(const QString& id) const;
    QString arch() const;
    static bool hasAutoSource(const EmuDef& e);

    void install(const QString& id, const QString& core = {}, bool force = false);
    void importExe(const QString& id, const QString& path);
    void checkUpdate(const QString& id);
    void checkAllUpdates();

signals:
    void status(const QString& msg);
    void progress(int percent);
    void finished(bool ok, const QString& msg);
    void updateInfo(const QString& id, const QString& latest, int state);
    void updatesChecked(int available);

private:
    using Done = std::function<void(bool, QString)>;
    using Resolved = std::function<void(bool ok, QString url, QString version, QString err)>;
    void resolve(const EmuDef& e, Resolved cb);
    void download(const QUrl& url, const QString& dest, Done done);
    void extract(const QString& archive, const QString& dir, Done done);
    void installExe(const EmuDef& e, Done done);
    void installCore(const QString& core, Done done);
    QNetworkAccessManager nam_;
    int pendingChecks_ = 0, foundUpdates_ = 0;
};
