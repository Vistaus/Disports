#pragma once

#include <QJsonObject>
#include <QObject>
#include <QString>
#include <QStringList>

#include <functional>

class CaptchaPrompt;
class QNetworkAccessManager;
struct HttpResult;

// Signing in with email (or phone number) and password, and the two-factor
// code when the account has one (docs.discord.food, "Authentication").
// Discord usually wants a captcha solved first (see Captcha.h).
class PasswordLogin : public QObject
{
    Q_OBJECT
    // "credentials", then "mfa" when a two-factor code is needed.
    Q_PROPERTY(QString step READ step NOTIFY changed)
    Q_PROPERTY(bool busy READ busy NOTIFY changed)
    // What went wrong, in words for the user.
    Q_PROPERTY(QString error READ error NOTIFY changed)
    // Other news, such as where the SMS code went.
    Q_PROPERTY(QString notice READ notice NOTIFY changed)
    // The two-factor methods the account has: "totp", "sms", "backup".
    Q_PROPERTY(QStringList methods READ methods NOTIFY changed)
    // An SMS code was asked for: 6 digits are that code.
    Q_PROPERTY(bool smsSent READ smsSent NOTIFY changed)

public:
    PasswordLogin(QNetworkAccessManager* nam, CaptchaPrompt* captcha, QObject* parent = nullptr);

    QString step() const { return m_step; }
    bool busy() const { return m_busy; }
    QString error() const { return m_error; }
    QString notice() const { return m_notice; }
    QStringList methods() const { return m_methods; }
    bool smsSent() const { return m_smsSent; }

    Q_INVOKABLE void login(const QString& login, const QString& password);
    Q_INVOKABLE void sendSmsCode();
    // The kind of code is told by its shape: 8 letters and digits are a
    // backup code, 6 digits the SMS code if one was sent, else the
    // authenticator app's.
    Q_INVOKABLE void verify(const QString& code);
    // Back to the start, forgetting everything entered.
    Q_INVOKABLE void reset();

    // Words for a sign-in request's answer that isn't a success (also used
    // by the QR login).
    static QString describe(const HttpResult& result);

signals:
    void changed();
    void tokenReceived(const QString& token);
    // Signed in, but Discord wants something done (a new password).
    void signedInNotice(const QString& text);

private:
    void post(const QString& path, const QJsonObject& body, std::function<void(const HttpResult&)> done);
    void withFingerprint(std::function<void()> then);
    void loginAnswered(const HttpResult& result);
    void verifyAnswered(const HttpResult& result);
    void finish(const QJsonObject& response);
    void setBusy(bool busy);
    void setError(const QString& error);

    QNetworkAccessManager* m_nam;
    CaptchaPrompt* m_captcha;
    QString m_fingerprint;
    QString m_ticket;
    QString m_loginInstance;
    QString m_step = QStringLiteral("credentials");
    QStringList m_methods;
    QString m_error;
    QString m_notice;
    bool m_busy = false;
    bool m_smsSent = false;
};
