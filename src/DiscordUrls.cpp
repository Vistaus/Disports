#include "DiscordUrls.h"

#include <QStringList>

#include "discord/network/DiscordAPI.hpp"

namespace DiscordUrls {

QString cdn()
{
    return QString::fromStdString(GetDiscordCDN());
}

QString userAvatar(Snowflake user, const std::string& hash, int size)
{
    if (hash.empty() || hash == "0") {
        // Default avatars are chosen by the new-username-system formula.
        const quint64 index = (quint64(user) >> 22) % 6;
        return cdn() + QStringLiteral("embed/avatars/%1.png").arg(index);
    }
    return cdn() + QStringLiteral("avatars/%1/%2.png?size=%3")
                       .arg(id(user), QString::fromStdString(hash))
                       .arg(size);
}

QString guildIcon(Snowflake guild, const std::string& hash, int size)
{
    if (hash.empty())
        return QString();
    return cdn() + QStringLiteral("icons/%1/%2.png?size=%3")
                       .arg(id(guild), QString::fromStdString(hash))
                       .arg(size);
}

QString channelIcon(Snowflake channel, const std::string& hash, int size)
{
    if (hash.empty())
        return QString();
    return cdn() + QStringLiteral("channel-icons/%1/%2.png?size=%3")
                       .arg(id(channel), QString::fromStdString(hash))
                       .arg(size);
}

QString emoji(Snowflake emojiId, bool animated, int size)
{
    return cdn() + QStringLiteral("emojis/%1.%2?size=%3")
                       .arg(id(emojiId), animated ? QStringLiteral("gif") : QStringLiteral("png"))
                       .arg(size);
}

QString initials(const QString& name)
{
    QString result;
    const QStringList words = name.split(QLatin1Char(' '), Qt::SkipEmptyParts);
    for (const QString& word : words) {
        result += word.at(0);
        if (result.size() == 2)
            break;
    }
    return result.isEmpty() ? name.left(2) : result;
}

}
