#include "Captcha.h"

#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QNetworkRequest>
#include <QPointer>

bool CaptchaChallenge::parse(int status, const QByteArray& body, CaptchaChallenge& challenge)
{
    if (status != 400)
        return false;
    const QJsonObject json = QJsonDocument::fromJson(body).object();
    if (!json.contains(QLatin1String("captcha_key")))
        return false;
    challenge.service = json.value(QLatin1String("captcha_service")).toString();
    challenge.siteKey = json.value(QLatin1String("captcha_sitekey")).toString();
    challenge.sessionId = json.value(QLatin1String("captcha_session_id")).toString();
    challenge.rqdata = json.value(QLatin1String("captcha_rqdata")).toString();
    challenge.rqtoken = json.value(QLatin1String("captcha_rqtoken")).toString();
    challenge.invisible = json.value(QLatin1String("should_serve_invisible")).toBool();
    return true;
}

void CaptchaChallenge::addHeaders(QNetworkRequest& request, const QString& solution) const
{
    request.setRawHeader("X-Captcha-Key", solution.toUtf8());
    if (!sessionId.isEmpty())
        request.setRawHeader("X-Captcha-Session-Id", sessionId.toUtf8());
    if (!rqtoken.isEmpty())
        request.setRawHeader("X-Captcha-Rqtoken", rqtoken.toUtf8());
}

void CaptchaPrompt::ask(const CaptchaChallenge& challenge, Answer answer)
{
    m_queue.push_back({challenge, std::move(answer)});
    if (m_queue.size() == 1) {
        emit changed();
        emit requested();
    }
}

void CaptchaPrompt::solve(const QString& solution)
{
    if (!solution.isEmpty())
        answer(solution);
}

void CaptchaPrompt::cancel()
{
    answer(QString());
}

void CaptchaPrompt::cancelAll()
{
    while (!m_queue.empty())
        answer(QString());
}

void CaptchaPrompt::answer(const QString& solution)
{
    if (m_queue.empty())
        return;
    Answer callback = std::move(m_queue.front().answer);
    m_queue.pop_front();
    emit changed();
    if (!m_queue.empty())
        emit requested();
    callback(solution);
}

void sendWithCaptcha(QNetworkAccessManager* nam, const QNetworkRequest& request, const QByteArray& verb,
                     const QByteArray& body, CaptchaPrompt* prompt, QObject* context,
                     std::function<void(const HttpResult&)> done,
                     std::function<void(QNetworkReply*)> started)
{
    QNetworkReply* reply = nam->sendCustomRequest(request, verb, body);
    if (started)
        started(reply);
    QPointer<CaptchaPrompt> guardedPrompt(prompt);
    QObject::connect(reply, &QNetworkReply::finished, context, [=]() {
        reply->deleteLater();
        HttpResult result;
        const QVariant status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute);
        result.status = status.isValid() ? status.toInt() : 0;
        result.body = reply->readAll();
        result.error = reply->error();
        result.errorString = reply->errorString();

        CaptchaChallenge challenge;
        // Only hCaptcha can be shown (see qml/CaptchaPage.qml).
        if (!guardedPrompt || !CaptchaChallenge::parse(result.status, result.body, challenge)
                || challenge.service != QLatin1String("hcaptcha")) {
            done(result);
            return;
        }
        QPointer<QObject> guardedContext(context);
        guardedPrompt->ask(challenge, [=](const QString& solution) {
            if (!guardedContext)
                return;
            if (solution.isEmpty()) {
                done(result);
                return;
            }
            QNetworkRequest retry = request;
            challenge.addHeaders(retry, solution);
            sendWithCaptcha(nam, retry, verb, body, guardedPrompt, guardedContext, done, started);
        });
    });
}
