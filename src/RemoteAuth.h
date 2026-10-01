#pragma once

#include <QByteArray>
#include <QObject>
#include <QString>
#include <QTimer>

class CaptchaPrompt;
class QJsonObject;
class QNetworkAccessManager;
class QWebSocket;
typedef struct evp_pkey_st EVP_PKEY;

// QR code login through Discord's remote-auth gateway (v2): the phone app
// scans a code, the user confirms, and Discord hands us an encrypted token.
class RemoteAuth : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QString qrImage READ qrImage NOTIFY qrImageChanged)
    Q_PROPERTY(QString status READ status NOTIFY statusChanged)
    Q_PROPERTY(bool busy READ busy NOTIFY statusChanged)
    Q_PROPERTY(bool failed READ failed NOTIFY statusChanged)

public:
    explicit RemoteAuth(QNetworkAccessManager* nam, QObject* parent = nullptr);
    ~RemoteAuth() override;

    QString qrImage() const { return m_qrImage; }
    QString status() const { return m_status; }
    bool busy() const { return m_busy; }
    bool failed() const { return m_failed; }

    void setCaptchaPrompt(CaptchaPrompt* prompt) { m_captcha = prompt; }

    Q_INVOKABLE void start();
    Q_INVOKABLE void stop();

signals:
    void qrImageChanged();
    void statusChanged();
    void tokenReceived(const QString& token);

private:
    void handleMessage(const QString& text);
    void send(const QJsonObject& object);
    void setStatus(const QString& status, bool busy, bool failed = false);
    void fail(const QString& message);
    void completeLogin(const QString& ticket);
    QByteArray decrypt(const QByteArray& base64) const;
    static QString renderQr(const QString& url);

    QNetworkAccessManager* m_nam;
    CaptchaPrompt* m_captcha = nullptr;
    QWebSocket* m_socket = nullptr;
    EVP_PKEY* m_key = nullptr;
    QString m_fingerprint;
    QTimer m_heartbeat;
    QString m_qrImage;
    QString m_status;
    bool m_busy = false;
    bool m_failed = false;
    bool m_finished = false;
};
