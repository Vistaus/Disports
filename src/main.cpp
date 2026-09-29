#include <QGuiApplication>
#include <QNetworkInformation>
#include <QQmlEngine>
#include <QQuickView>
#include <QTimer>

#include <memory>

#include "Session.h"

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
// once connected, DISPORTS_SEND_MESSAGE=<text> then sends a message to it.
void scheduleTestActions(Session* session)
{
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
    });
}

}

int main(int argc, char* argv[])
{
    QGuiApplication app(argc, argv);
    // Data lives in ~/.local/share/disports.jukfiuu, the app's writable
    // namespace on Ubuntu Touch.
    QCoreApplication::setApplicationName(QStringLiteral("disports.jukfiuu"));
    QCoreApplication::setApplicationVersion(QStringLiteral(DISPORTS_VERSION));

    Session session;
    qmlRegisterSingletonInstance("Disports.Core", 1, 0, "Session", &session);
    qmlRegisterUncreatableType<Session>("Disports.Core", 1, 0, "SessionPhase",
                                        QStringLiteral("Use the Session singleton"));

    QQuickView view;
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
