#include "DiscordRpc.h"
#include "DiscordConfig.h"
#include "I18n.h"
#include <QCoreApplication>
#include <QDateTime>
#include <QJsonArray>
#include <QJsonDocument>
#include <QSettings>
#include <QtEndian>

DiscordRpc& DiscordRpc::instance() {
    static DiscordRpc inst;
    return inst;
}

DiscordRpc::DiscordRpc(QObject* p) : QObject(p), sock_(new QLocalSocket(this)) {
    retry_.setSingleShot(true);
    connect(&retry_, &QTimer::timeout, this, [this] { cand_ = candidates(); idx_ = 0; connectNext(); });
    connect(sock_, &QLocalSocket::connected, this, &DiscordRpc::onConnected);
    connect(sock_, &QLocalSocket::readyRead, this, &DiscordRpc::onReadyRead);
    connect(sock_, &QLocalSocket::disconnected, this, &DiscordRpc::onDisconnected);
    connect(sock_, &QLocalSocket::errorOccurred, this, [this](QLocalSocket::LocalSocketError) {
        if (enabled_ && !handshaked_) connectNext();
    });
    connect(qApp, &QCoreApplication::aboutToQuit, this, &DiscordRpc::shutdown);
}

QString DiscordRpc::effectiveAppId() const {
    const QString s = QSettings().value("discord/appId").toString().trimmed();
    return s.isEmpty() ? QString::fromUtf8(DiscordConfig::AppId).trimmed() : s;
}

void DiscordRpc::setStatus(const QString& s) {
    if (s == status_) return;
    status_ = s;
    emit statusChanged(s);
}

QStringList DiscordRpc::candidates() const {
    QStringList out;
#ifdef Q_OS_WIN
    for (int i = 0; i < 10; ++i) out << QString("discord-ipc-%1").arg(i);
#else
    QStringList bases;
    for (const char* v : {"XDG_RUNTIME_DIR", "TMPDIR", "TMP", "TEMP"}) {
        const QString e = qEnvironmentVariable(v);
        if (!e.isEmpty() && !bases.contains(e)) bases << e;
    }
    if (!bases.contains("/tmp")) bases << "/tmp";
    for (const QString& b : bases)
        for (const QString& sub : {QString(), QString("/app/com.discordapp.Discord"), QString("/snap.discord")})
            for (int i = 0; i < 10; ++i) out << QString("%1%2/discord-ipc-%3").arg(b, sub).arg(i);
#endif
    return out;
}

void DiscordRpc::connectNext() {
    if (!enabled_) return;
    if (effectiveAppId().isEmpty()) { setStatus(T("Informe o Application ID do Discord.")); return; }
    if (idx_ >= cand_.size()) {
        idx_ = 0;
        setStatus(T("Discord não encontrado (está aberto?). Tentando de novo…"));
        retry_.start(15000);
        return;
    }
    sock_->abort();
    handshaked_ = false;
    sock_->connectToServer(cand_[idx_++]);
}

void DiscordRpc::send(quint32 op, const QJsonObject& obj) {
    const QByteArray body = QJsonDocument(obj).toJson(QJsonDocument::Compact);
    QByteArray frame(8, 0);
    qToLittleEndian<quint32>(op, frame.data());
    qToLittleEndian<quint32>(quint32(body.size()), frame.data() + 4);
    sock_->write(frame + body);
}

void DiscordRpc::onConnected() {
    retry_.stop();
    connectedAppId_ = effectiveAppId();
    handshaked_ = true;
    send(0, {{"v", 1}, {"client_id", connectedAppId_}});
    setStatus(T("Conectando ao Discord…"));
}

void DiscordRpc::onReadyRead() {
    rx_ += sock_->readAll();
    while (rx_.size() >= 8) {
        const quint32 op = qFromLittleEndian<quint32>(rx_.constData());
        const quint32 len = qFromLittleEndian<quint32>(rx_.constData() + 4);
        if (quint32(rx_.size()) < 8 + len) break;
        const QJsonObject o = QJsonDocument::fromJson(rx_.mid(8, len)).object();
        const QByteArray raw = rx_.mid(8, len);
        rx_.remove(0, 8 + len);

        if (op == 3) {
            QByteArray f(8, 0);
            qToLittleEndian<quint32>(4, f.data());
            qToLittleEndian<quint32>(quint32(raw.size()), f.data() + 4);
            sock_->write(f + raw);
        } else if (op == 2) {
            setStatus(T("Discord recusou: %1").arg(o["message"].toString()));
            enabled_ = false;
            retry_.stop();
            sock_->disconnectFromServer();
        } else if (op == 1) {
            if (o["evt"].toString() == "READY") {
                ready_ = true;
                setStatus(T("Conectado ao Discord (%1)").arg(o["data"].toObject()["user"].toObject()["username"].toString()));
                pushActivity();
            } else if (o["evt"].toString() == "ERROR") {
                setStatus(T("Discord recusou: %1").arg(o["data"].toObject()["message"].toString()));
            }
        }
    }
}

void DiscordRpc::onDisconnected() {
    ready_ = false;
    handshaked_ = false;
    rx_.clear();
    if (enabled_) { setStatus(T("Discord desconectado. Tentando de novo…")); retry_.start(5000); }
}

QJsonObject DiscordRpc::buildActivity(bool* any) const {
    QSettings s;
    QJsonObject a;
    auto txt = [](const QString& t) { return t.size() >= 2 ? t : QString(); };
    const bool showTimer = s.value("discord/showTimer", true).toBool();

    if (active_ > 0) {
        const bool showGame = s.value("discord/showGame", true).toBool();
        const bool showSys = s.value("discord/showConsole", true).toBool();
        const QString details = showGame ? QString::fromUtf8(DiscordConfig::PlayDetails).arg(playTitle_) : QString::fromUtf8(DiscordConfig::PlayDetailsHidden);
        a["details"] = txt(details);
        if (showSys) a["state"] = txt(QString::fromUtf8(DiscordConfig::PlayState).arg(playSysName_));
        if (showTimer) a["timestamps"] = QJsonObject{{"start", playStart_}};
    } else {
        if (!s.value("discord/showBrowsing", true).toBool()) { *any = false; return {}; }
        a["details"] = txt(QString::fromUtf8(DiscordConfig::IdleDetails));
        a["state"] = txt(QString::fromUtf8(DiscordConfig::IdleState).arg(games_));
    }
    if (a["details"].toString().isEmpty()) a.remove("details");
    if (a["state"].toString().isEmpty()) a.remove("state");

    QJsonObject assets;
    if (*DiscordConfig::LargeImageKey) { assets["large_image"] = QString::fromUtf8(DiscordConfig::LargeImageKey); assets["large_text"] = QString::fromUtf8(DiscordConfig::LargeImageText); }
    if (active_ > 0 && *DiscordConfig::SmallImagePrefix && s.value("discord/showConsole", true).toBool() && !playSysId_.isEmpty()) {
        assets["small_image"] = QString::fromUtf8(DiscordConfig::SmallImagePrefix) + playSysId_;
        assets["small_text"] = playSysName_;
    }
    if (!assets.isEmpty()) a["assets"] = assets;

    QJsonArray buttons;
    const char* labels[2] = {DiscordConfig::Button1Label, DiscordConfig::Button2Label};
    const char* urls[2] = {DiscordConfig::Button1Url, DiscordConfig::Button2Url};
    for (int i = 0; i < 2; ++i)
        if (*labels[i] && *urls[i]) buttons.append(QJsonObject{{"label", QString::fromUtf8(labels[i])}, {"url", QString::fromUtf8(urls[i])}});
    if (!buttons.isEmpty()) a["buttons"] = buttons;

    *any = true;
    return a;
}

void DiscordRpc::pushActivity() {
    if (!ready_) return;
    bool any = false;
    const QJsonObject act = buildActivity(&any);
    QJsonObject args{{"pid", qint64(QCoreApplication::applicationPid())}};
    if (any) args["activity"] = act;
    send(1, {{"cmd", "SET_ACTIVITY"}, {"args", args}, {"nonce", QString::number(++nonce_)}});
}

void DiscordRpc::applySettings() {
    const bool en = QSettings().value("discord/enabled", false).toBool();
    if (!en) {
        if (ready_) {
            send(1, {{"cmd", "SET_ACTIVITY"}, {"args", QJsonObject{{"pid", qint64(QCoreApplication::applicationPid())}}},
                     {"nonce", QString::number(++nonce_)}});
            sock_->flush();
            sock_->waitForBytesWritten(300);
        }
        enabled_ = false;
        retry_.stop();
        if (sock_->state() != QLocalSocket::UnconnectedState) { sock_->flush(); sock_->disconnectFromServer(); }
        setStatus(T("Desativado."));
        return;
    }
    const bool wasEnabled = enabled_;
    enabled_ = true;
    if (ready_ && connectedAppId_ == effectiveAppId()) { pushActivity(); return; }
    retry_.stop();
    if (sock_->state() != QLocalSocket::UnconnectedState) sock_->abort();
    Q_UNUSED(wasEnabled);
    cand_ = candidates();
    idx_ = 0;
    connectNext();
}

void DiscordRpc::setBrowsing(int n) { games_ = n; if (active_ == 0) pushActivity(); }

void DiscordRpc::setPlaying(const QString& title, const QString& id, const QString& name) {
    ++active_;
    playTitle_ = title; playSysId_ = id; playSysName_ = name;
    playStart_ = QDateTime::currentSecsSinceEpoch();
    pushActivity();
}

void DiscordRpc::stopPlaying(int n) {
    active_ = qMax(0, active_ - 1);
    games_ = n;
    pushActivity();
}

void DiscordRpc::reconnect() {
    retry_.stop();
    if (sock_->state() != QLocalSocket::UnconnectedState) sock_->abort();
    ready_ = false;
    handshaked_ = false;
    if (!QSettings().value("discord/enabled", false).toBool()) { setStatus(T("Desativado.")); return; }
    enabled_ = true;
    cand_ = candidates();
    idx_ = 0;
    connectNext();
}

void DiscordRpc::shutdown() {
    if (ready_) {
        QJsonObject args{{"pid", qint64(QCoreApplication::applicationPid())}};
        send(1, {{"cmd", "SET_ACTIVITY"}, {"args", args}, {"nonce", QString::number(++nonce_)}});
        sock_->flush();
        sock_->waitForBytesWritten(300);
    }
    enabled_ = false;
    retry_.stop();
    sock_->abort();
}
