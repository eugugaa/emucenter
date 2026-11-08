#include "Emulators.h"
#include "FileUtil.h"
#include "I18n.h"
#include <QDesktopServices>
#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkReply>
#include <QProcess>
#include <QRegularExpression>
#include <QSettings>
#include <QStandardPaths>
#include <QSysInfo>
#include <QUrl>

static QString defaultRoot() {
    return QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) + "/emulators";
}

EmulatorManager::EmulatorManager(QObject* p) : QObject(p) { QDir().mkpath(root()); }

QString EmulatorManager::root() const { return QSettings().value("emuRoot", defaultRoot()).toString(); }
void EmulatorManager::setRoot(const QString& d) { QSettings().setValue("emuRoot", d); QDir().mkpath(d); }

QString EmulatorManager::emuDir(const QString& id) const {
    const QString v = QSettings().value("emuDir/" + id).toString();
    return v.isEmpty() ? root() + "/" + id : v;
}
void EmulatorManager::setEmuDir(const QString& id, const QString& dir) {
    QSettings().setValue("emuDir/" + id, dir);
}

QString EmulatorManager::arch() const {
    const QString o = QSettings().value("arch", "auto").toString();
    if (o == "x64" || o == "arm64") return o;
    const QString c = QSysInfo::currentCpuArchitecture().toLower();
    return (c.contains("arm") || c.contains("aarch")) ? "arm64" : "x64";
}

QString EmulatorManager::installedVersion(const QString& id) const {
    return QSettings().value("ver/" + id).toString();
}

bool EmulatorManager::hasAutoSource(const EmuDef& e) {
    return e.id == "dolphin" || !e.githubRepo.isEmpty() || !e.directUrl.isEmpty();
}

QString EmulatorManager::findExe(const QString& id) const {
    const EmuDef* e = Catalog::emu(id);
    if (!e) return {};
    const QString manual = QSettings().value("emu/" + id).toString();
    if (!manual.isEmpty() && QFile::exists(manual)) return manual;

    QDirIterator it(emuDir(id), e->exeNames, QDir::Files, QDirIterator::Subdirectories);
    if (it.hasNext()) return it.next();
    for (const QString& n : e->exeNames) {
        const QString p = QStandardPaths::findExecutable(n);
        if (!p.isEmpty()) return p;
    }
    return {};
}

QString EmulatorManager::corePath(const QString& core) const {
    const QString ra = findExe("retroarch");
    if (ra.isEmpty()) return {};
    const QStringList dirs = {QFileInfo(ra).absolutePath() + "/cores", emuDir("retroarch") + "/cores"};
    const QStringList exts = {".dll", ".so", ".dylib"};
    for (const QString& d : dirs)
        for (const QString& ext : exts)
            if (QFile::exists(d + "/" + core + ext)) return d + "/" + core + ext;
    return {};
}

bool EmulatorManager::available(const EmuLink& l) const {
    if (findExe(l.emuId).isEmpty()) return false;
    return l.core.isEmpty() || !corePath(l.core).isEmpty();
}

bool EmulatorManager::buildCommand(const EmuLink& l, const QString& rom, QString& prog, QStringList& args) const {
    const EmuDef* e = Catalog::emu(l.emuId);
    prog = findExe(l.emuId);
    if (!e || prog.isEmpty()) return false;
    args.clear();
    for (QString a : e->args) {
        a.replace("{rom}", rom);
        a.replace("{core}", corePath(l.core));
        args << a;
    }
    return true;
}

void EmulatorManager::importExe(const QString& id, const QString& path) {
    QSettings().setValue("emu/" + id, path);
}

void EmulatorManager::download(const QUrl& url, const QString& dest, Done done) {
    QNetworkRequest rq(url);
    rq.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::NoLessSafeRedirectPolicy);
    rq.setHeader(QNetworkRequest::UserAgentHeader, "EmuCenter/0.2");
    QNetworkReply* r = nam_.get(rq);
    const QString name = QFileInfo(dest).fileName();
    connect(r, &QNetworkReply::downloadProgress, this, [this, name](qint64 a, qint64 b) {
        const int pct = b > 0 ? int(a * 100 / b) : -1;
        emit progress(pct);
        emit status(pct >= 0 ? T("Baixando %1… %2%").arg(name).arg(pct) : T("Baixando %1… %2 MB").arg(name).arg(a / 1048576));
    });
    connect(r, &QNetworkReply::finished, this, [r, dest, done]() {
        r->deleteLater();
        if (r->error() != QNetworkReply::NoError) return done(false, r->errorString());
        QDir().mkpath(QFileInfo(dest).absolutePath());
        QFile f(dest);
        if (!f.open(QIODevice::WriteOnly)) return done(false, T("Não foi possível gravar %1").arg(dest));
        f.write(r->readAll());
        done(true, dest);
    });
}

void EmulatorManager::extract(const QString& archive, const QString& dir, Done done) {
    emit status(T("Extraindo…"));
    emit progress(-1);
    auto* p = new QProcess(this);
    connect(p, &QProcess::finished, this, [p, archive, done](int code, QProcess::ExitStatus) {
        const bool ok = code == 0;
        if (ok) QFile::remove(archive);
        done(ok, ok ? QString() : T("Falha ao extrair: ") + QString::fromLocal8Bit(p->readAllStandardError()));
        p->deleteLater();
    });
    connect(p, &QProcess::errorOccurred, this, [p, done](QProcess::ProcessError e) {
        if (e == QProcess::FailedToStart) { done(false, T("Comando 'tar' não encontrado.")); p->deleteLater(); }
    });
    p->start("tar", {"-xf", archive, "-C", dir});
}

void EmulatorManager::resolve(const EmuDef& e, Resolved cb) {
    QNetworkRequest rq;
    rq.setHeader(QNetworkRequest::UserAgentHeader, "EmuCenter/0.2");
    rq.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::NoLessSafeRedirectPolicy);
    const QString arch = this->arch();

    if (e.id == "dolphin") {
        rq.setUrl(QUrl("https://dolphin-emu.org/download/?nocr=true"));
        QNetworkReply* r = nam_.get(rq);
        connect(r, &QNetworkReply::finished, this, [r, arch, cb]() {
            r->deleteLater();
            if (r->error() != QNetworkReply::NoError) return cb(false, "", "", r->errorString());
            const QString html = QString::fromUtf8(r->readAll());
            static const QRegularExpression re(R"(https://dl\.dolphin-emu\.org/releases/([^/"'\s]+)/dolphin-\1-(x64|arm64)\.7z)",
                                               QRegularExpression::CaseInsensitiveOption);
            QString fallbackUrl, fallbackVer;
            auto it = re.globalMatch(html);
            while (it.hasNext()) {
                const auto m = it.next();
                const QString a = m.captured(2).toLower();
                if (a == arch) return cb(true, m.captured(0), m.captured(1), "");
                if (a == "x64" && fallbackUrl.isEmpty()) { fallbackUrl = m.captured(0); fallbackVer = m.captured(1); }
            }
            if (!fallbackUrl.isEmpty()) return cb(true, fallbackUrl, fallbackVer, "");
            cb(false, "", "", T("Não encontrei o download na página do Dolphin."));
        });
        return;
    }
    if (!e.githubRepo.isEmpty()) {
        rq.setUrl(QUrl("https://api.github.com/repos/" + e.githubRepo + "/releases/latest"));
        QNetworkReply* r = nam_.get(rq);
        connect(r, &QNetworkReply::finished, this, [r, e, arch, cb]() {
            r->deleteLater();
            if (r->error() != QNetworkReply::NoError) return cb(false, "", "", r->errorString());
            const auto o = QJsonDocument::fromJson(r->readAll()).object();
            const QString tag = o["tag_name"].toString();
            const auto assets = o["assets"].toArray();
            auto find = [&](const QString& rx) -> QString {
                if (rx.isEmpty()) return {};
                const QRegularExpression re(rx, QRegularExpression::CaseInsensitiveOption);
                for (const auto& a : assets) {
                    const QJsonObject ao = a.toObject();
                    if (re.match(ao["name"].toString()).hasMatch()) return ao["browser_download_url"].toString();
                }
                return {};
            };
            QString url = arch == "arm64" ? find(e.assetRegexArm) : QString();
            if (url.isEmpty()) url = find(e.assetRegex);
            if (url.isEmpty()) return cb(false, "", tag, "manual");
            cb(true, url, tag, "");
        });
        return;
    }
    if (!e.directUrl.isEmpty()) return cb(true, e.directUrl, "latest", "");
    cb(false, "", "", "manual");
}

void EmulatorManager::installExe(const EmuDef& e, Done done) {
#ifndef Q_OS_WIN

    QDesktopServices::openUrl(QUrl(e.homepage));
    return done(false, T("Abri o site oficial de %1 — instale lá e use 'Importar'.").arg(e.name));
#endif
    emit status(T("Procurando a última versão de %1…").arg(e.name));
    emit progress(-1);
    resolve(e, [this, e, done](bool ok, QString url, QString ver, QString err) {
        if (!ok) {
            if (err == "manual") {
                QDesktopServices::openUrl(QUrl(e.homepage));
                return done(false, T("Não há download automático de %1; abri o site oficial. Baixe lá e use 'Importar'.").arg(e.name));
            }
            return done(false, err);
        }
        const QString dir = emuDir(e.id);
        const QString file = dir + "/" + QFileInfo(QUrl(url).path()).fileName();
        download(QUrl(url), file, [this, e, dir, file, ver, done](bool ok2, QString m) {
            if (!ok2) return done(false, m);
            extract(file, dir, [this, e, ver, done](bool ok3, QString m3) {
                if (ok3) {
                    QSettings().setValue("ver/" + e.id, ver);
                    if (e.id == "dolphin") {
                        const QString exe = findExe("dolphin");
                        if (!exe.isEmpty()) FileUtil::touch(QFileInfo(exe).absolutePath() + "/portable.txt");
                    }
                }
                done(ok3, m3);
            });
        });
    });
}

void EmulatorManager::installCore(const QString& core, Done done) {
    const QString dir = emuDir("retroarch") + "/cores";
    const QString file = dir + "/" + core + ".zip";
    download(QUrl("https://buildbot.libretro.com/nightly/windows/x86_64/latest/" + core + ".dll.zip"), file,
             [this, dir, file, done](bool ok, QString msg) {
                 if (!ok) return done(false, msg);
                 extract(file, dir, done);
             });
}

void EmulatorManager::install(const QString& id, const QString& core, bool force) {
    const EmuDef* e = Catalog::emu(id);
    if (!e) return;
    auto finish = [this](bool ok, QString msg) { emit progress(100); emit finished(ok, msg); };
    auto doCore = [this, core, finish]() {
        if (core.isEmpty() || !corePath(core).isEmpty()) return finish(true, T("Pronto!"));
        emit status(T("Baixando core %1…").arg(core));
        installCore(core, [finish](bool ok, QString m) { finish(ok, ok ? T("Pronto!") : m); });
    };
    if (!force && !findExe(id).isEmpty()) return doCore();
    installExe(*e, [finish, doCore](bool ok, QString m) { ok ? doCore() : finish(false, m); });
}

void EmulatorManager::checkUpdate(const QString& id) {
    const EmuDef* e = Catalog::emu(id);
    if (!e || !hasAutoSource(*e) || findExe(id).isEmpty()) return;
    ++pendingChecks_;
    resolve(*e, [this, id](bool ok, QString, QString ver, QString) {
        if (ok || !ver.isEmpty()) {
            const QString cur = installedVersion(id);
            const int st = cur.isEmpty() ? 2 : (cur == ver ? 0 : 1);
            if (st == 1) ++foundUpdates_;
            emit updateInfo(id, ver, st);
        }
        if (--pendingChecks_ == 0) { emit updatesChecked(foundUpdates_); foundUpdates_ = 0; }
    });
}

void EmulatorManager::checkAllUpdates() {
    foundUpdates_ = 0;
    bool any = false;
    for (const auto& e : Catalog::emulators())
        if (hasAutoSource(e) && !findExe(e.id).isEmpty()) { any = true; checkUpdate(e.id); }
    if (!any) emit updatesChecked(0);
}
