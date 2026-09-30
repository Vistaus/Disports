#pragma once

#include <QQmlNetworkAccessManagerFactory>

// A disk cache for profile pictures, server icons and group icons, so they
// show offline too. Their URLs contain a hash of the picture, so a cached
// one never goes stale.
class ImageCacheFactory : public QQmlNetworkAccessManagerFactory
{
public:
    QNetworkAccessManager* create(QObject* parent) override;

    // Whether a URL is one of the cached kinds.
    static bool isCached(const class QUrl& url);
};
