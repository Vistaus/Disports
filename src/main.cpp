#include <QDir>
#include <QGuiApplication>
#include <QNetworkInformation>
#include <QQmlEngine>
#include <QQuickView>

#include "ImageCache.h"
#include "Session.h"
#include "media/GstVideoPlayer.h"
#ifdef DISPORTS_TEST_HOOKS
#include "testing/TestHooks.h"
#endif

namespace {

// Lets the session drop a dead gateway connection as soon as the network
// goes away, and reconnect when it is back.
void watchNetwork(Session* session)
{
    if (!QNetworkInformation::loadBackendByFeatures(QNetworkInformation::Feature::Reachability))
        return; // no backend: heartbeats notice it instead
    QNetworkInformation* info = QNetworkInformation::instance();
    auto update = [session](QNetworkInformation::Reachability reachability) {
        session->connection()->setNetworkOnline(reachability != QNetworkInformation::Reachability::Disconnected);
    };
    update(info->reachability());
    QObject::connect(info, &QNetworkInformation::reachabilityChanged, session, update);
}

void registerQmlTypes(Session* session)
{
    qmlRegisterSingletonInstance("Disports.Core", 1, 0, "Session", session);
    qmlRegisterType<GstVideoPlayer>("Disports.Core", 1, 0, "GstVideoPlayer");
    qmlRegisterUncreatableType<Session>("Disports.Core", 1, 0, "SessionPhase",
                                        QStringLiteral("Use the Session singleton"));
    qmlRegisterUncreatableType<CallManager>("Disports.Core", 1, 0, "CallState",
                                            QStringLiteral("Use Session.call"));
}

}

int main(int argc, char* argv[])
{
    QGuiApplication app(argc, argv);
    // Also the data folder name: ~/.local/share/disports.jukfiuu is the
    // app's writable space on Ubuntu Touch.
    QCoreApplication::setApplicationName(QStringLiteral("disports.jukfiuu"));
    QCoreApplication::setApplicationVersion(QStringLiteral(DISPORTS_VERSION));

    // Qt plugins the click ships itself (WebP), see third_party/CMakeLists.txt.
    const QDir bundledLibs(QCoreApplication::applicationDirPath() + QStringLiteral("/../lib/" DISPORTS_ARCH_TRIPLET));
    if (bundledLibs.exists())
        QCoreApplication::addLibraryPath(bundledLibs.absoluteFilePath(QStringLiteral("plugins")));

    Session session;
    registerQmlTypes(&session);

    QQuickView view;
    ImageCacheFactory imageCache;
    view.engine()->setNetworkAccessManagerFactory(&imageCache);
    view.setResizeMode(QQuickView::SizeRootObjectToView);
    view.setTitle(QStringLiteral("Disports"));
    view.resize(450, 800);
#ifdef DISPORTS_TEST_HOOKS
    TestHooks::install(&session, &view);
#endif
    QObject::connect(view.engine(), &QQmlEngine::quit, &app, &QCoreApplication::quit);
    view.loadFromModule("Disports", "Main");
    if (view.status() == QQuickView::Error)
        return 1;
    view.show();

    watchNetwork(&session);
    session.start();
    return app.exec();
}
