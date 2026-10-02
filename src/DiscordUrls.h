#pragma once

#include <QString>

#include <string>

#include "discord/models/Snowflake.hpp"

// CDN URLs for images QML loads directly.
namespace DiscordUrls {

inline QString id(Snowflake sf)
{
    return QString::number(quint64(sf));
}

inline Snowflake fromId(const QString& text)
{
    return Snowflake(text.toULongLong());
}

QString cdn();
QString userAvatar(Snowflake user, const std::string& hash, int size = 64);
QString guildIcon(Snowflake guild, const std::string& hash, int size = 96);
QString channelIcon(Snowflake channel, const std::string& hash, int size = 64);
QString emoji(Snowflake emoji, bool animated, int size = 48);
// A sticker's image: PNG (APNG stickers show their first frame) or GIF.
QString sticker(Snowflake sticker, bool gif, int size = 160);
// A Lottie sticker's animation (JSON).
QString lottieSticker(Snowflake sticker);

// Up to two initials for a server without an icon ("Ubuntu Touch" -> "UT").
QString initials(const QString& name);

}
