#include "Updater.h"
#include "Config.h"
#include "FileUtil.h"
#include "I18n.h"
#include <QCoreApplication>
#include <QDesktopServices>
#include <QDir>
#include <QDirIterator>
#include <QFileInfo>
#include <QNetworkReply>
#include <QProcess>
#include <QRegularExpression>
#include <QSettings>
#include <QStandardPaths>
#include <QTimer>
#include <QUrl>
#include <QVector>

Updater::Updater(QObject* p) : QObject(p) {}

QString Updater::currentVersion() { return QString::fromLatin1(Config::AppVersion); }

QString Updater::base() const {
    return QSettings().value("update/base", "https://github.com").toString();
}

QString Updater::workDir() const {
    return QStandardPaths::writableLocation(QStandardPaths::TempLocation) + "/emucenter_update";
}

static void split(const QString& v, QVector<int>& nums, QString& suffix) {
    QString s = v.trimmed();
    if (s.startsWith('v') || s.startsWith('V')) s = s.mid(1);
    static const QRegularExpression re(R"(^(\d+(?:\.\d+)*)(.*)$)");
    const auto m = re.match(s);
    nums.clear();
    if (!m.hasMatch()) { suffix = s.toLower(); return; }
    const QStringList parts = m.captured(1).split('.');
    for (const QString& p : parts) nums << p.toInt();
    suffix = m.captured(2).trimmed().toLower();
}

int Updater::compareVersions(const QString& a, const QString& b) {
    QVector<int> na, nb;
    QString sa, sb;
    split(a, na, sa);
    split(b, nb, sb);
    const int n = qMax(na.size(), nb.size());
    for (int i = 0; i < n; ++i) {
        const int x = i < na.size() ? na[i] : 0;
        const int y = i < nb.size() ? nb[i] : 0;
        if (x != y) return x < y ? -1 : 1;
    }
    auto rank = [](const QString& s) {
        if (s.isEmpty()) return 1;
        static const QRegularExpression pre(R"(alpha|beta|rc|pre|dev|-)");
        return s.contains(pre) ? 0 : 2;
    };
    const int ra = rank(sa), rb = rank(sb);
    if (ra != rb) return ra < rb ? -1 : 1;
    if (sa == sb) return 0;
    return sa < sb ? -1 : 1;
}

void Updater::latestTag(bool includePre, std::function<void(QString, QString)> cb) {
    const QString repo = QString::fromLatin1(Config::GithubRepo);
    QNetworkRequest rq;
    rq.setHeader(QNetworkRequest::UserAgentHeader, "EmuCenter-Updater");
    if (!includePre) {
        rq.setUrl(QUrl(base() + "/" + repo + "/releases/latest"));
        rq.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::ManualRedirectPolicy);
        QNetworkReply* r = nam_.get(rq);
        connect(r, &QNetworkReply::finished, this, [r, cb]() {
            r->deleteLater();
            const QUrl loc = r->url().resolved(QUrl(QString::fromUtf8(r->rawHeader("Location"))));
            const QString path = QUrl::fromPercentEncoding(loc.path().toUtf8());
            const int i = path.indexOf("/releases/tag/");
            if (i >= 0) return cb(path.mid(i + 14), QString());
            if (r->error() != QNetworkReply::NoError && r->error() != QNetworkReply::ProtocolUnknownError && r->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt() == 0)
                return cb(QString(), r->errorString());
            cb(QString(), QString());
        });
        return;
    }
    rq.setUrl(QUrl(base() + "/" + repo + "/releases"));
    rq.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::NoLessSafeRedirectPolicy);
    QNetworkReply* r = nam_.get(rq);
    connect(r, &QNetworkReply::finished, this, [r, cb]() {
        r->deleteLater();
        if (r->error() != QNetworkReply::NoError) return cb(QString(), r->errorString());
        const QString html = QString::fromUtf8(r->readAll());
        static const QRegularExpression re(R"(/releases/tag/([^"'?#<>\s]+))");
        QString best;
        auto it = re.globalMatch(html);
        while (it.hasNext()) {
            const QString t = QUrl::fromPercentEncoding(it.next().captured(1).toUtf8());
            if (best.isEmpty() || compareVersions(t, best) > 0) best = t;
        }
        cb(best, QString());
    });
}

void Updater::check(bool includePre) {
    if (busy_) return;
    busy_ = true;
    latestTag(includePre, [this](QString tag, QString err) {
        busy_ = false;
        newer_ = false;
        tag_ = tag;
        if (!err.isEmpty()) return emit checked(false, false, QString(), T("Não consegui verificar atualizações: %1").arg(err));
        if (tag.isEmpty()) return emit checked(true, false, QString(), T("Nenhuma versão foi publicada ainda no GitHub."));
        newer_ = compareVersions(tag, currentVersion()) > 0;
        emit checked(true, newer_, tag, newer_ ? T("Nova versão disponível: %1 (você usa %2).").arg(tag, currentVersion())
                                               : T("Você já está na versão mais recente (%1).").arg(currentVersion()));
    });
}

void Updater::install() {
    if (busy_ || tag_.isEmpty()) return;
    busy_ = true;
    QDir(workDir()).removeRecursively();
    QDir().mkpath(workDir());
    const QString asset = QString::fromLatin1(Config::UpdateAsset);
    const QUrl url(base() + "/" + QString::fromLatin1(Config::GithubRepo) + "/releases/download/" + QUrl::toPercentEncoding(tag_) + "/" + asset);
    QNetworkRequest rq(url);
    rq.setHeader(QNetworkRequest::UserAgentHeader, "EmuCenter-Updater");
    rq.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::NoLessSafeRedirectPolicy);
    QNetworkReply* r = nam_.get(rq);
    connect(r, &QNetworkReply::downloadProgress, this, [this](qint64 a, qint64 b) {
        emit progress(b > 0 ? int(a * 100 / b) : -1, T("Baixando atualização…"));
    });
    connect(r, &QNetworkReply::finished, this, [this, r, asset]() {
        r->deleteLater();
        const int code = r->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        if (r->error() != QNetworkReply::NoError || code >= 400) {
            busy_ = false;
            return emit failed(code == 404 ? T("A versão %1 não tem o arquivo %2 anexado.").arg(tag_, asset) : r->errorString());
        }
        const QString zip = workDir() + "/" + asset;
        if (!FileUtil::writeAll(zip, r->readAll())) {
            busy_ = false;
            return emit failed(T("Não foi possível gravar %1").arg(zip));
        }
        extractAndApply(zip);
    });
}

static QString findRoot(const QString& dir) {
#ifdef Q_OS_WIN
    const QString exe = QStringLiteral("EmuCenter.exe");
#else
    const QString exe = QStringLiteral("EmuCenter");
#endif
    QDirIterator it(dir, {exe}, QDir::Files, QDirIterator::Subdirectories);
    return it.hasNext() ? QFileInfo(it.next()).absolutePath() : QString();
}

void Updater::extractAndApply(const QString& zip) {
    emit progress(-1, T("Extraindo…"));
    const QString out = workDir() + "/extracted";
    QDir().mkpath(out);
    auto* p = new QProcess(this);
    auto afterExtract = [this, out, p](bool ok, const QString& err) {
        p->deleteLater();
        if (!ok) { busy_ = false; return emit failed(T("Falha ao extrair: ") + err); }
        const QString root = findRoot(out);
        if (root.isEmpty()) { busy_ = false; return emit failed(T("O pacote não contém o EmuCenter.exe.")); }
        if (!applyUpdate(root)) busy_ = false;
    };
    connect(p, &QProcess::finished, this, [p, zip, out, afterExtract](int code, QProcess::ExitStatus) {
        if (code == 0) return afterExtract(true, QString());
        auto* q = new QProcess(p->parent());
        QObject::connect(q, &QProcess::finished, p, [q, afterExtract](int c2, QProcess::ExitStatus) {
            const QString e = QString::fromLocal8Bit(q->readAllStandardError());
            q->deleteLater();
            afterExtract(c2 == 0, e);
        });
        QObject::connect(q, &QProcess::errorOccurred, p, [q, afterExtract](QProcess::ProcessError) {
            q->deleteLater();
            afterExtract(false, T("Comando 'tar' não encontrado."));
        });
        q->start("unzip", {"-o", "-q", zip, "-d", out});
    });
    connect(p, &QProcess::errorOccurred, this, [p, zip, out, afterExtract](QProcess::ProcessError e) {
        if (e != QProcess::FailedToStart) return;
        auto* q = new QProcess(p->parent());
        QObject::connect(q, &QProcess::finished, p, [q, afterExtract](int c2, QProcess::ExitStatus) {
            const QString err = QString::fromLocal8Bit(q->readAllStandardError());
            q->deleteLater();
            afterExtract(c2 == 0, err);
        });
        QObject::connect(q, &QProcess::errorOccurred, p, [q, afterExtract](QProcess::ProcessError) {
            q->deleteLater();
            afterExtract(false, T("Comando 'tar' não encontrado."));
        });
        q->start("unzip", {"-o", "-q", zip, "-d", out});
    });
    p->start("tar", {"-xf", zip, "-C", out});
}

bool Updater::applyUpdate(const QString& srcDir) {
#ifdef Q_OS_WIN
    const QString dst = QDir::toNativeSeparators(QCoreApplication::applicationDirPath());
    const QString src = QDir::toNativeSeparators(srcDir);
    const QString exe = QFileInfo(QCoreApplication::applicationFilePath()).fileName();
    const QString bat = workDir() + "/apply_update.bat";
    QStringList l;
    l << "@echo off" << "set \"SRC=" + src + "\"" << "set \"DST=" + dst + "\""
      << "set \"PID=" + QString::number(QCoreApplication::applicationPid()) + "\"" << "set N=0"
      << ":wait" << "tasklist /FI \"PID eq %PID%\" 2>nul | find \"%PID%\" >nul"
      << "if errorlevel 1 goto copy" << "timeout /t 1 /nobreak >nul" << "goto wait"
      << ":copy" << "xcopy \"%SRC%\\*\" \"%DST%\" /E /Y /I /Q >nul" << "if not errorlevel 1 goto done"
      << "set /a N=%N%+1" << "if %N% GEQ 8 goto done" << "timeout /t 1 /nobreak >nul" << "goto copy"
      << ":done" << "start \"\" \"%DST%\\" + exe + "\"" << "del \"%~f0\"";
    if (!FileUtil::writeAll(bat, (l.join("\r\n") + "\r\n").toLocal8Bit())) {
        emit failed(T("Não foi possível gravar %1").arg(bat));
        return false;
    }
    emit progress(100, T("Reiniciando para concluir a atualização…"));
    QProcess::startDetached("cmd.exe", {"/c", "start", "", "/min", QDir::toNativeSeparators(bat)});
    QTimer::singleShot(400, qApp, &QCoreApplication::quit);
    return true;
#else
    emit progress(100, T("Pacote extraído em %1. A instalação automática só existe no Windows.").arg(srcDir));
    QDesktopServices::openUrl(QUrl::fromLocalFile(srcDir));
    busy_ = false;
    return true;
#endif
}
