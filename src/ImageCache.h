#pragma once

#include <QQmlNetworkAccessManagerFactory>

// A disk cache for the pictures that identify people and places: profile
// pictures, server icons and group DM icons. Nothing else (attachments,
// previews, emoji) is cached.
//
// Discord puts a hash of the picture in these URLs
// (avatars/<user>/<hash>.png, icons/<guild>/<hash>.png), so a changed
// picture has a new URL: a cached one never needs checking, and is kept
// until the cache is full. They show offline, and cost no requests online.
class ImageCacheFactory : public QQmlNetworkAccessManagerFactory
{
public:
    QNetworkAccessManager* create(QObject* parent) override;

    // Whether a URL is one of the cached kinds.
    static bool isCached(const class QUrl& url);
};
