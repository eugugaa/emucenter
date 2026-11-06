#pragma once
#include <QJsonObject>
#include <QLocalSocket>
#include <QObject>
#include <QTimer>

// Discord Rich Presence via IPC local (sem SDK). Reconecta sozinho se o Discord abrir depois do app.
class DiscordRpc : public QObject {
    Q_OBJECT
public:
    static DiscordRpc& instance();
    void applySettings();                                   // lê as configurações e (des)conecta
    void setBrowsing(int gameCount);
    void setPlaying(const QString& title, const QString& systemId, const QString& systemName);
    void stopPlaying(int gameCount);
    void shutdown();                                        // limpa a presença e fecha
    QString effectiveAppId() const;                         // configurações > valor embutido no .exe
    bool isConnected() const { return ready_; }
    QString status() const { return status_; }
signals:
    void statusChanged(const QString& text);
private:
    explicit DiscordRpc(QObject* parent = nullptr);
    QStringList candidates() const;
    void connectNext();
    void onConnected();
    void onReadyRead();
    void onDisconnected();
    void send(quint32 op, const QJsonObject& obj);
    void pushActivity();
    QJsonObject buildActivity(bool* any) const;
    void setStatus(const QString& s);

    QLocalSocket* sock_;
    QTimer retry_;
    QByteArray rx_;
    QStringList cand_;
    int idx_ = 0;
    bool enabled_ = false, ready_ = false, handshaked_ = false;
    QString status_, connectedAppId_;
    int active_ = 0, games_ = 0;
    qint64 nonce_ = 0, playStart_ = 0;
    QString playTitle_, playSysId_, playSysName_;
};
