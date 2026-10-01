#include "TestHooks.h"

#include <QCoreApplication>
#include <QQmlContext>
#include <QQuickView>
#include <QTimer>
#include <QUrl>

#include <memory>

#include "Session.h"

void TestHooks::install(Session* session, QQuickView* view)
{
    auto* hooks = new TestHooks(session, view);
    view->rootContext()->setContextProperty(QStringLiteral("testHooks"), hooks);
}

TestHooks::TestHooks(Session* session, QQuickView* view)
    : QObject(view)
    , m_session(session)
    , m_view(view)
    , m_openChannel(qEnvironmentVariable("DISPORTS_OPEN_CHANNEL"))
    , m_videoUrl(qEnvironmentVariable("DISPORTS_PLAY_VIDEO"))
    , m_captchaToken(qEnvironmentVariable("DISPORTS_CAPTCHA_TOKEN"))
{
    const QString api = qEnvironmentVariable("DISPORTS_API_URL");
    const QString cdn = qEnvironmentVariable("DISPORTS_CDN_URL");
    if (!api.isEmpty() || !cdn.isEmpty())
        session->setServerUrls(api, cdn);

    const int width = qEnvironmentVariableIntValue("DISPORTS_WIDTH");
    const int height = qEnvironmentVariableIntValue("DISPORTS_HEIGHT");
    if (width > 0 && height > 0)
        view->resize(width, height);

    scheduleScreenshot();
    scheduleChannelActions();
    scheduleFolder();
    scheduleLogin();

    if (!m_captchaToken.isEmpty()) {
        connect(session->captcha(), &CaptchaPrompt::requested, this, [this]() {
            QTimer::singleShot(300, this, [this]() { m_session->captcha()->solve(m_captchaToken); });
        });
    }
}

void TestHooks::scheduleLogin()
{
    const QString login = qEnvironmentVariable("DISPORTS_PASSWORD_LOGIN");
    if (login.isEmpty())
        return;
    PasswordLogin* password = m_session->passwordLogin();
    QTimer::singleShot(2000, this, [password, login]() {
        password->login(login.section(QLatin1Char(':'), 0, 0), login.section(QLatin1Char(':'), 1));
    });
    const QString mfa = qEnvironmentVariable("DISPORTS_MFA");
    if (mfa.isEmpty())
        return;
    auto sent = std::make_shared<bool>(false);
    connect(password, &PasswordLogin::changed, this, [password, mfa, sent]() {
        if (*sent || password->step() != QLatin1String("mfa") || password->busy())
            return;
        *sent = true;
        QTimer::singleShot(500, password, [password, mfa]() {
            password->verify(mfa.section(QLatin1Char(':'), 0, 0), mfa.section(QLatin1Char(':'), 1));
        });
    });
}

void TestHooks::scheduleScreenshot()
{
    const QString spec = qEnvironmentVariable("DISPORTS_SCREENSHOT");
    if (spec.isEmpty())
        return;
    const QString path = spec.section(QLatin1Char(':'), 0, 0);
    const int delay = spec.section(QLatin1Char(':'), 1, 1).toInt();
    QTimer::singleShot(delay > 0 ? delay : 3000, this, [this, path]() {
        const QImage image = m_view->grabWindow();
        const bool saved = !image.isNull() && image.save(path);
        qInfo("Screenshot %s: %dx%d, %s", qPrintable(path), image.width(), image.height(),
              saved ? "saved" : "FAILED");
        QCoreApplication::quit();
    });
}

void TestHooks::scheduleChannelActions()
{
    if (m_openChannel.isEmpty())
        return;
    auto connection = std::make_shared<QMetaObject::Connection>();
    *connection = connect(m_session->connection(), &GatewayConnection::connectedChanged, this, [this, connection]() {
        if (!m_session->connected())
            return;
        disconnect(*connection);
        m_session->openChannel(m_openChannel);

        const QString text = qEnvironmentVariable("DISPORTS_SEND_MESSAGE");
        if (!text.isEmpty())
            QTimer::singleShot(1500, this, [this, text]() { m_session->sender()->send(text); });

        const QString file = qEnvironmentVariable("DISPORTS_SEND_FILE");
        if (!file.isEmpty()) {
            const QString caption = qEnvironmentVariable("DISPORTS_SEND_FILE_TEXT");
            QTimer::singleShot(2000, this, [this, file, caption]() {
                m_session->sender()->sendFile(QUrl::fromLocalFile(file).toString(), caption);
            });
        }

        if (qEnvironmentVariableIsSet("DISPORTS_START_CALL"))
            QTimer::singleShot(1500, this, [this]() { m_session->call()->start(m_openChannel); });

        // Then straight to another call.
        const QString next = qEnvironmentVariable("DISPORTS_SWITCH_CALL");
        if (!next.isEmpty())
            QTimer::singleShot(4000, this, [this, next]() { m_session->call()->start(next); });
    });
}

void TestHooks::scheduleFolder()
{
    const QString folder = qEnvironmentVariable("DISPORTS_EXPAND_FOLDER");
    if (folder.isEmpty())
        return;
    auto connection = std::make_shared<QMetaObject::Connection>();
    *connection = connect(m_session->guilds(), &GuildListModel::countChanged, this, [this, folder, connection]() {
        if (m_session->guilds()->rowCount() == 0)
            return;
        disconnect(*connection);
        QTimer::singleShot(0, this, [this, folder]() { m_session->guilds()->toggleFolder(folder); });
    });
}
