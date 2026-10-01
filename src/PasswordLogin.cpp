#include "PasswordLogin.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QNetworkAccessManager>
#include <QNetworkRequest>
#include <QUrl>
#include <QtMath>

#include "Captcha.h"
#include "backend/QtHttpClient.h"
#include "discord/network/DiscordAPI.hpp"

namespace {

QJsonObject jsonOf(const QByteArray& body)
{
    return QJsonDocument::fromJson(body).object();
}

// The first field error of a form error ("errors": {"login": {"_errors":
// [{"code": ..., "message": ...}]}}), anywhere in the tree.
QJsonObject firstFieldError(const QJsonObject& object)
{
    const QJsonArray errors = object.value(QLatin1String("_errors")).toArray();
    if (!errors.isEmpty())
        return errors.first().toObject();
    for (auto it = object.begin(); it != object.end(); ++it) {
        if (it.value().isObject()) {
            const QJsonObject found = firstFieldError(it.value().toObject());
            if (!found.isEmpty())
                return found;
        }
    }
    return QJsonObject();
}

}

PasswordLogin::PasswordLogin(QNetworkAccessManager* nam, CaptchaPrompt* captcha, QObject* parent)
    : QObject(parent)
    , m_nam(nam)
    , m_captcha(captcha)
{
}

void PasswordLogin::reset()
{
    m_ticket.clear();
    m_loginInstance.clear();
    m_methods.clear();
    m_step = QStringLiteral("credentials");
    m_error.clear();
    m_notice.clear();
    m_busy = false;
    emit changed();
}

void PasswordLogin::setBusy(bool busy)
{
    m_busy = busy;
    emit changed();
}

void PasswordLogin::setError(const QString& error)
{
    m_error = error;
    m_busy = false;
    emit changed();
}

void PasswordLogin::post(const QString& path, const QJsonObject& body, std::function<void(const HttpResult&)> done)
{
    QNetworkRequest request(QUrl(QString::fromStdString(GetDiscordAPI()) + path));
    QtHttpClient::applyClientHeaders(request);
    request.setHeader(QNetworkRequest::ContentTypeHeader, QByteArray("application/json"));
    if (!m_fingerprint.isEmpty())
        request.setRawHeader("X-Fingerprint", m_fingerprint.toUtf8());
    sendWithCaptcha(m_nam, request, "POST", QJsonDocument(body).toJson(QJsonDocument::Compact), m_captcha, this,
                    std::move(done));
}

// Discord ties a sign-in to a fingerprint from its experiments; without
// one, the login is tried anyway.
void PasswordLogin::withFingerprint(std::function<void()> then)
{
    if (!m_fingerprint.isEmpty()) {
        then();
        return;
    }
    QNetworkRequest request(QUrl(QString::fromStdString(GetDiscordAPI()) + QStringLiteral("experiments")));
    QtHttpClient::applyClientHeaders(request);
    sendWithCaptcha(m_nam, request, "GET", QByteArray(), nullptr, this, [this, then](const HttpResult& result) {
        m_fingerprint = jsonOf(result.body).value(QLatin1String("fingerprint")).toString();
        then();
    });
}

void PasswordLogin::login(const QString& login, const QString& password)
{
    if (m_busy)
        return;
    if (login.trimmed().isEmpty() || password.isEmpty()) {
        setError(tr("Enter your email or phone number and your password."));
        return;
    }
    m_error.clear();
    m_notice.clear();
    setBusy(true);
    const QJsonObject body{
        {QStringLiteral("login"), login.trimmed()},
        {QStringLiteral("password"), password},
        {QStringLiteral("undelete"), false},
    };
    withFingerprint([this, body]() {
        post(QStringLiteral("auth/login"), body, [this](const HttpResult& result) { loginAnswered(result); });
    });
}

void PasswordLogin::loginAnswered(const HttpResult& result)
{
    const QJsonObject response = jsonOf(result.body);
    if (result.status != 200) {
        setError(describe(result));
        return;
    }
    if (!response.value(QLatin1String("mfa")).toBool()) {
        finish(response);
        return;
    }
    m_ticket = response.value(QLatin1String("ticket")).toString();
    m_loginInstance = response.value(QLatin1String("login_instance_id")).toString();
    m_methods.clear();
    for (const char* method : {"totp", "sms", "backup"}) {
        if (response.value(QLatin1String(method)).toBool())
            m_methods.append(QLatin1String(method));
    }
    if (m_methods.isEmpty()) {
        // Only a security key (WebAuthn).
        setError(tr("This account only has a security key for two-factor authentication, which Disports can't use. "
                    "Sign in with the QR code instead."));
        return;
    }
    m_step = QStringLiteral("mfa");
    setBusy(false);
}

void PasswordLogin::sendSmsCode()
{
    if (m_busy || m_ticket.isEmpty())
        return;
    m_error.clear();
    setBusy(true);
    post(QStringLiteral("auth/mfa/sms/send"), {{QStringLiteral("ticket"), m_ticket}}, [this](const HttpResult& result) {
        if (result.status != 200) {
            setError(describe(result));
            return;
        }
        const QString phone = jsonOf(result.body).value(QLatin1String("phone")).toString();
        m_notice = phone.isEmpty() ? tr("A code was sent to your phone.") : tr("A code was sent to %1.").arg(phone);
        setBusy(false);
    });
}

void PasswordLogin::verify(const QString& method, const QString& code)
{
    if (m_busy || m_ticket.isEmpty() || !m_methods.contains(method))
        return;
    const QString trimmed = QString(code).remove(QLatin1Char(' ')).remove(QLatin1Char('-'));
    if (trimmed.isEmpty()) {
        setError(tr("Enter the code."));
        return;
    }
    m_error.clear();
    setBusy(true);
    const QJsonObject body{
        {QStringLiteral("ticket"), m_ticket},
        {QStringLiteral("login_instance_id"), m_loginInstance},
        {QStringLiteral("code"), trimmed},
    };
    post(QStringLiteral("auth/mfa/") + method, body, [this](const HttpResult& result) { verifyAnswered(result); });
}

void PasswordLogin::verifyAnswered(const HttpResult& result)
{
    if (result.status == 200) {
        finish(jsonOf(result.body));
        return;
    }
    const QJsonObject response = jsonOf(result.body);
    const QString fieldCode = firstFieldError(response.value(QLatin1String("errors")).toObject())
                                  .value(QLatin1String("code")).toString();
    // The ticket only lasts a few minutes.
    if (fieldCode.contains(QLatin1String("TICKET"), Qt::CaseInsensitive)) {
        reset();
        setError(tr("Signing in took too long. Please start again."));
        return;
    }
    if (response.value(QLatin1String("code")).toInt() == 60008) {
        setError(tr("That code isn't right. Check it and try again."));
        return;
    }
    setError(describe(result));
}

void PasswordLogin::finish(const QJsonObject& response)
{
    const QString token = response.value(QLatin1String("token")).toString();
    if (token.isEmpty()) {
        setError(tr("Discord didn't send a sign-in token. Please try again."));
        return;
    }
    const QJsonArray actions = response.value(QLatin1String("required_actions")).toArray();
    reset();
    emit tokenReceived(token);
    if (actions.contains(QJsonValue(QStringLiteral("update_password"))))
        emit signedInNotice(tr("Discord asks you to change your password. You can do that on discord.com."));
}

QString PasswordLogin::describe(const HttpResult& result)
{
    const QJsonObject response = jsonOf(result.body);
    if (result.status == 0)
        return tr("Couldn't reach Discord. Check your internet connection and try again.");

    CaptchaChallenge challenge;
    if (CaptchaChallenge::parse(result.status, result.body, challenge)) {
        if (challenge.service != QLatin1String("hcaptcha"))
            return tr("Discord asked for a kind of captcha Disports can't show. Sign in with the QR code instead.");
        return tr("Discord needs the captcha solved before you can sign in.");
    }
    if (result.status == 429) {
        const int seconds = qMax(1, qCeil(response.value(QLatin1String("retry_after")).toDouble()));
        return tr("Too many attempts. Wait %1 s and try again.").arg(seconds);
    }
    if (result.status == 403 && response.contains(QLatin1String("suspended_user_token")))
        return tr("Discord has suspended this account. See discord.com to find out why and to appeal.");
    if (result.status >= 500)
        return tr("Discord is having trouble right now. Try again in a few minutes.");

    switch (response.value(QLatin1String("code")).toInt()) {
    case 20011:
    case 20013:
        return tr("This account is disabled or scheduled for deletion. Restore it in the official Discord app first.");
    case 70007:
        return tr("Discord sent a code to your phone to confirm this new sign-in. Disports can't take that code yet: "
                  "sign in with your email address or the QR code instead.");
    }

    const QJsonObject field = firstFieldError(response.value(QLatin1String("errors")).toObject());
    const QString code = field.value(QLatin1String("code")).toString();
    if (code == QLatin1String("INVALID_LOGIN"))
        return tr("The email, phone number or password isn't right.");
    if (code == QLatin1String("ACCOUNT_LOGIN_VERIFICATION_EMAIL"))
        return tr("Discord sent you an email to confirm this new sign-in. Open it, confirm, then sign in here again.");

    // Discord's own words, when there are any.
    const QString message = field.value(QLatin1String("message")).toString();
    if (!message.isEmpty())
        return message;
    const QString general = response.value(QLatin1String("message")).toString();
    if (!general.isEmpty())
        return tr("Discord said: %1").arg(general);
    return tr("Signing in didn't work (error %1). Please try again.").arg(result.status);
}
