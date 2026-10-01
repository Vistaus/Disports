#pragma once

#include <QObject>
#include <QString>

class QQuickView;
class Session;

// Hooks for the headless runs in tools/test, driven by environment
// variables. Only built with -DDISPORTS_TEST_HOOKS=ON.
//
//   DISPORTS_API_URL, DISPORTS_CDN_URL   use the fake server
//   DISPORTS_WIDTH, DISPORTS_HEIGHT      window size
//   DISPORTS_SCREENSHOT=path[:ms]        save a screenshot and quit
//   DISPORTS_OPEN_CHANNEL=id             open a channel once connected, then:
//     DISPORTS_SEND_MESSAGE=text         send a message
//     DISPORTS_SEND_FILE=path            send a file (DISPORTS_SEND_FILE_TEXT)
//     DISPORTS_START_CALL=1              call it, or join the voice channel
//   DISPORTS_EXPAND_FOLDER=id            expand a server folder
//   DISPORTS_PLAY_VIDEO=[gif:|preview:]url  open a video (TestHooks.qml)
//   DISPORTS_CAPTCHA_TOKEN=solution      answer captchas with it (no page)
//   DISPORTS_PASSWORD_LOGIN=login:password  sign in with a password, then
//     DISPORTS_MFA=code                  give the two-factor code
class TestHooks : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QString openChannel MEMBER m_openChannel CONSTANT)
    Q_PROPERTY(QString videoUrl MEMBER m_videoUrl CONSTANT)
    Q_PROPERTY(bool answersCaptchas READ answersCaptchas CONSTANT)

public:
    // Before the QML is loaded and the session started.
    static void install(Session* session, QQuickView* view);

    bool answersCaptchas() const { return !m_captchaToken.isEmpty(); }

private:
    TestHooks(Session* session, QQuickView* view);
    void scheduleScreenshot();
    void scheduleChannelActions();
    void scheduleFolder();
    void scheduleLogin();

    Session* m_session;
    QQuickView* m_view;
    QString m_openChannel;
    QString m_videoUrl;
    QString m_captchaToken;
};
