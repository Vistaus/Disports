#include <QDir>
#include <QGuiApplication>
#include <QNetworkInformation>
#include <QQmlContext>
#include <QQmlDebuggingEnabler>
#include <QQmlEngine>
#include <QQuickView>
#include <QTimer>
#include <QUrl>

#include <memory>

#include "ImageCache.h"
#include "Session.h"
#include "media/GstVideoPlayer.h"

namespace {

// Follows the system's view of the network so the session can drop a dead
// gateway connection at once and reconnect as soon as the network is back.
void watchNetwork(Session* session)
{
    if (!QNetworkInformation::loadBackendByFeatures(QNetworkInformation::Feature::Reachability))
        return; // no backend: assume online and rely on heartbeats
    QNetworkInformation* info = QNetworkInformation::instance();
    auto update = [session](QNetworkInformation::Reachability reachability) {
        session->setNetworkOnline(reachability != QNetworkInformation::Reachability::Disconnected);
    };
    update(info->reachability());
    QObject::connect(info, &QNetworkInformation::reachabilityChanged, session, update);
}

// DISPORTS_SCREENSHOT=path[:delay-ms] saves a screenshot and quits; used to
// check the UI in headless test runs.
void scheduleScreenshot(QQuickView* view)
{
    const QString spec = qEnvironmentVariable("DISPORTS_SCREENSHOT");
    if (spec.isEmpty())
        return;
    const QString path = spec.section(QLatin1Char(':'), 0, 0);
    const int delay = spec.section(QLatin1Char(':'), 1, 1).toInt();
    QTimer::singleShot(delay > 0 ? delay : 3000, view, [view, path]() {
        const QImage image = view->grabWindow();
        const bool saved = !image.isNull() && image.save(path);
        qInfo("Screenshot %s: %dx%d, %s", qPrintable(path), image.width(), image.height(),
              saved ? "saved" : "FAILED");
        QCoreApplication::quit();
    });
}

// Test hooks for headless runs: DISPORTS_OPEN_CHANNEL=<id> opens a channel
// once connected, DISPORTS_SEND_MESSAGE=<text> then sends a message to it,
// DISPORTS_START_CALL=1 calls it (or joins it, for a voice channel),
// DISPORTS_SEND_FILE=<path> sends that file to it (with the text in
// DISPORTS_SEND_FILE_TEXT),
// DISPORTS_EXPAND_FOLDER=<id> expands a server folder. DISPORTS_PLAY_VIDEO=<url>
// (read by Main.qml) opens the media viewer with a video, logged in or not;
// "gif:<url>" opens it as a looping GIF, "preview:<url>" shows the chat's
// preview of a GIF (with "Play GIFs in the chat" on).
void scheduleTestActions(Session* session)
{
    const QString folder = qEnvironmentVariable("DISPORTS_EXPAND_FOLDER");
    if (!folder.isEmpty()) {
        QObject::connect(session->guilds(), &GuildListModel::countChanged, session, [session, folder]() {
            static bool done = false;
            if (!done && session->guilds()->rowCount() > 0) {
                done = true;
                QTimer::singleShot(0, session, [session, folder]() { session->guilds()->toggleFolder(folder); });
            }
        });
    }

    const QString channel = qEnvironmentVariable("DISPORTS_OPEN_CHANNEL");
    if (channel.isEmpty())
        return;
    auto connection = std::make_shared<QMetaObject::Connection>();
    *connection = QObject::connect(session, &Session::connectedChanged, session, [session, channel, connection]() {
        if (!session->connected())
            return;
        QObject::disconnect(*connection);
        session->openChannel(channel);
        const QString text = qEnvironmentVariable("DISPORTS_SEND_MESSAGE");
        if (!text.isEmpty())
            QTimer::singleShot(1500, session, [session, text]() { session->sendMessage(text); });
        const QString file = qEnvironmentVariable("DISPORTS_SEND_FILE");
        if (!file.isEmpty())
            QTimer::singleShot(2000, session, [session, file]() {
                session->sendAttachment(QUrl::fromLocalFile(file).toString(), qEnvironmentVariable("DISPORTS_SEND_FILE_TEXT"));
            });
        if (qEnvironmentVariableIsSet("DISPORTS_START_CALL"))
            QTimer::singleShot(1500, session, [session, channel]() { session->call()->start(channel); });
    });
}

}

int main(int argc, char* argv[])
{

    QGuiApplication app(argc, argv);

    // DISPORTS_QML_PROFILE_PORT=<port>: waits for Qt's QML profiler
    // (qmlprofiler --attach) there; for finding slow bindings (tools/test).
    // After the application object, before the QML engine.
    const int profilePort = qEnvironmentVariableIntValue("DISPORTS_QML_PROFILE_PORT");
    if (profilePort > 0) {
        QQmlDebuggingEnabler::enableDebugging(true);
        QQmlDebuggingEnabler::startTcpDebugServer(profilePort, QQmlDebuggingEnabler::WaitForClient);
    }
    // Data lives in ~/.local/share/disports.jukfiuu, the app's writable
    // namespace on Ubuntu Touch.
    QCoreApplication::setApplicationName(QStringLiteral("disports.jukfiuu"));
    QCoreApplication::setApplicationVersion(QStringLiteral(DISPORTS_VERSION));

    // Qt plugins that are not part of the Ubuntu Touch images yet (WebP
    // images) ship with the click in lib/<triplet>/plugins; see
    // third_party/CMakeLists.txt.
    const QString bundledLibs = QDir(QCoreApplication::applicationDirPath())
                                    .absoluteFilePath(QStringLiteral("../lib/" DISPORTS_ARCH_TRIPLET));
    if (QDir(bundledLibs).exists())
        QCoreApplication::addLibraryPath(bundledLibs + QStringLiteral("/plugins"));

    Session session;
    qmlRegisterSingletonInstance("Disports.Core", 1, 0, "Session", &session);
    qmlRegisterType<GstVideoPlayer>("Disports.Core", 1, 0, "GstVideoPlayer");
    qmlRegisterUncreatableType<Session>("Disports.Core", 1, 0, "SessionPhase",
                                        QStringLiteral("Use the Session singleton"));
    qmlRegisterUncreatableType<CallManager>("Disports.Core", 1, 0, "CallState",
                                            QStringLiteral("Use Session.call"));

    QQuickView view;
    // Profile pictures and server icons are kept on disk; see ImageCache.h.
    ImageCacheFactory imageCache;
    view.engine()->setNetworkAccessManagerFactory(&imageCache);
    // DISPORTS_TEST_SCROLL=1: the open chat scrolls up by itself, and the
    // frame rate, slowest frames and rows changing height are logged
    // ("scroll-test: ..."), to measure scrolling (tools/test).
    // DISPORTS_OPEN_CHANNEL also opens the chat page on narrow screens.
    view.rootContext()->setContextProperty(QStringLiteral("testOpenChannel"),
                                           qEnvironmentVariable("DISPORTS_OPEN_CHANNEL"));
    view.rootContext()->setContextProperty(QStringLiteral("testScroll"),
                                           qEnvironmentVariableIsSet("DISPORTS_TEST_SCROLL"));
    if (qEnvironmentVariableIsSet("DISPORTS_TEST_SCROLL") && QGuiApplication::primaryScreen())
        qInfo("scroll-test: the screen says it refreshes at %.1f Hz", QGuiApplication::primaryScreen()->refreshRate());
    view.rootContext()->setContextProperty(QStringLiteral("testVideoUrl"),
                                           qEnvironmentVariable("DISPORTS_PLAY_VIDEO"));
    view.setResizeMode(QQuickView::SizeRootObjectToView);
    view.setTitle(QStringLiteral("Disports"));
    QObject::connect(view.engine(), &QQmlEngine::quit, &app, &QCoreApplication::quit);
    view.loadFromModule("Disports", "Main");
    if (view.status() == QQuickView::Error)
        return 1;
    view.resize(qEnvironmentVariableIntValue("DISPORTS_WIDTH") ?: 450,
                qEnvironmentVariableIntValue("DISPORTS_HEIGHT") ?: 800);
    view.show();

    watchNetwork(&session);
    scheduleTestActions(&session);
    session.start();
    scheduleScreenshot(&view);

    return app.exec();
}
