#pragma once

#include <QByteArray>
#include <QNetworkReply>
#include <QObject>
#include <QString>

#include <deque>
#include <functional>

class QNetworkAccessManager;
class QNetworkRequest;

// Discord's captchas (see docs.discord.food, "CAPTCHA Handling"): any
// request may be answered with a 400 carrying "captcha_key". The request is
// then sent again with the solution in X-Captcha-Key, and the session id
// and rqtoken it came with.
struct CaptchaChallenge
{
    QString service;
    QString siteKey;
    QString sessionId;
    QString rqdata;  // must go into the challenge (hCaptcha Enterprise)
    QString rqtoken;
    bool invisible = false;

    // Whether this reply is a captcha challenge.
    static bool parse(int status, const QByteArray& body, CaptchaChallenge& challenge);
    void addHeaders(QNetworkRequest& request, const QString& solution) const;
};

// The captchas waiting for the user, one at a time, for the captcha page
// (qml/CaptchaPage.qml): Session.captcha.
class CaptchaPrompt : public QObject
{
    Q_OBJECT
    Q_PROPERTY(bool active READ active NOTIFY changed)
    Q_PROPERTY(QString siteKey READ siteKey NOTIFY changed)
    Q_PROPERTY(QString rqdata READ rqdata NOTIFY changed)
    Q_PROPERTY(bool invisible READ invisible NOTIFY changed)

public:
    // The solution, or an empty string when the user gave up.
    using Answer = std::function<void(const QString& solution)>;

    using QObject::QObject;

    bool active() const { return !m_queue.empty(); }
    QString siteKey() const { return active() ? m_queue.front().challenge.siteKey : QString(); }
    QString rqdata() const { return active() ? m_queue.front().challenge.rqdata : QString(); }
    bool invisible() const { return active() && m_queue.front().challenge.invisible; }

    void ask(const CaptchaChallenge& challenge, Answer answer);
    Q_INVOKABLE void solve(const QString& solution);
    Q_INVOKABLE void cancel();
    // Signing out: nothing waits for an answer any more.
    void cancelAll();

signals:
    void changed();
    // A captcha to show (the first, or the next one in line).
    void requested();

private:
    void answer(const QString& solution);

    struct Pending {
        CaptchaChallenge challenge;
        Answer answer;
    };
    std::deque<Pending> m_queue;
};

struct HttpResult
{
    int status = 0; // 0: no answer (see error)
    QByteArray body;
    QNetworkReply::NetworkError error = QNetworkReply::NoError;
    QString errorString;
};

// Sends a request; when Discord answers with a captcha, asks `prompt` and
// sends it again with the solution. `done` gets the last answer (the
// captcha challenge itself when the user gave up). `started` sees every
// reply sent, for progress and aborting. Nothing is called once `context`
// is gone.
void sendWithCaptcha(QNetworkAccessManager* nam, const QNetworkRequest& request, const QByteArray& verb,
                     const QByteArray& body, CaptchaPrompt* prompt, QObject* context,
                     std::function<void(const HttpResult&)> done,
                     std::function<void(QNetworkReply*)> started = {});
