#include "ImageCache.h"

#include <QDateTime>
#include <QNetworkAccessManager>
#include <QNetworkDiskCache>
#include <QNetworkRequest>
#include <QStandardPaths>
#include <QUrl>

namespace {

constexpr qint64 MaxCacheBytes = 50 * 1024 * 1024;

// Keeps the content-addressed pictures as long as there is room; they never
// change under the same URL.
class PictureDiskCache : public QNetworkDiskCache
{
public:
    using QNetworkDiskCache::QNetworkDiskCache;

    QIODevice* prepare(const QNetworkCacheMetaData& metaData) override
    {
        QNetworkCacheMetaData kept = metaData;
        kept.setExpirationDate(QDateTime::currentDateTimeUtc().addYears(10));
        kept.setSaveToDisk(true);
        return QNetworkDiskCache::prepare(kept);
    }
};

class ImageNetworkAccessManager : public QNetworkAccessManager
{
public:
    using QNetworkAccessManager::QNetworkAccessManager;

protected:
    QNetworkReply* createRequest(Operation op, const QNetworkRequest& original, QIODevice* data) override
    {
        QNetworkRequest request = original;
        if (op == GetOperation && ImageCacheFactory::isCached(request.url())) {
            request.setAttribute(QNetworkRequest::CacheLoadControlAttribute, QNetworkRequest::PreferCache);
            request.setAttribute(QNetworkRequest::CacheSaveControlAttribute, true);
        } else {
            request.setAttribute(QNetworkRequest::CacheLoadControlAttribute, QNetworkRequest::AlwaysNetwork);
            request.setAttribute(QNetworkRequest::CacheSaveControlAttribute, false);
        }
        return QNetworkAccessManager::createRequest(op, request, data);
    }
};

}

bool ImageCacheFactory::isCached(const QUrl& url)
{
    const QString path = url.path();
    return path.startsWith(QLatin1String("/avatars/"))          // profile pictures
           || path.startsWith(QLatin1String("/embed/avatars/")) // default ones
           || path.startsWith(QLatin1String("/icons/"))          // server icons
           || path.startsWith(QLatin1String("/channel-icons/")); // group DM icons
}

QNetworkAccessManager* ImageCacheFactory::create(QObject* parent)
{
    // Called once per QML network thread; the caches share the directory.
    auto* manager = new ImageNetworkAccessManager(parent);
    auto* cache = new PictureDiskCache(manager);
    cache->setCacheDirectory(QStandardPaths::writableLocation(QStandardPaths::CacheLocation)
                             + QStringLiteral("/pictures"));
    cache->setMaximumCacheSize(MaxCacheBytes);
    manager->setCache(cache);
    return manager;
}
