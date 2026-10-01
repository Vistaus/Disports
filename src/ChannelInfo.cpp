#include "ChannelInfo.h"

#include <QCoreApplication>

#include "discord/DiscordInstance.hpp"
#include "discord/state/ProfileCache.hpp"

#include "DiscordUrls.h"
#include "models/ChannelListModel.h"

namespace {

QString qstr(const std::string& s)
{
    return QString::fromStdString(s);
}

QString tr(const char* text)
{
    return QCoreApplication::translate("ChannelInfo", text);
}

QString typeName(const Channel& channel)
{
    switch (channel.m_channelType) {
    case Channel::DM:         return tr("Direct message");
    case Channel::GROUPDM:    return tr("Group");
    case Channel::VOICE:      return tr("Voice channel");
    case Channel::STAGEVOICE: return tr("Stage channel");
    case Channel::NEWS:       return tr("Announcement channel");
    case Channel::FORUM:      return tr("Forum");
    case Channel::MEDIA:      return tr("Media channel");
    case Channel::NEWSTHREAD:
    case Channel::PUBTHREAD:
    case Channel::PRIVTHREAD: return tr("Thread");
    default:                  return tr("Text channel");
    }
}

QString kind(const Channel& channel)
{
    switch (channel.m_channelType) {
    case Channel::DM:      return QStringLiteral("dm");
    case Channel::GROUPDM: return QStringLiteral("group");
    default:               return QStringLiteral("channel");
    }
}

QVariantList members(DiscordInstance& instance, const Channel& channel)
{
    QVariantList list;
    for (Snowflake id : channel.m_recipients) {
        Profile* profile = GetProfileCache()->LookupProfile(id, "", "", "", false);
        const QString username = profile ? qstr(profile->m_name) : QString();
        const QString name = profile && !profile->m_globalName.empty() ? qstr(profile->m_globalName) : username;
        list.append(QVariantMap{
            {QStringLiteral("id"), DiscordUrls::id(id)},
            {QStringLiteral("name"), name},
            {QStringLiteral("username"), username},
            {QStringLiteral("avatarUrl"), DiscordUrls::userAvatar(id, profile ? profile->m_avatarlnk : std::string())},
            {QStringLiteral("blocked"), instance.IsUserBlocked(id)},
        });
    }
    return list;
}

}

QVariantMap describeChannel(DiscordInstance& instance, const Channel& channel)
{
    QVariantMap info{
        {QStringLiteral("id"), DiscordUrls::id(channel.m_snowflake)},
        {QStringLiteral("kind"), kind(channel)},
        {QStringLiteral("typeName"), typeName(channel)},
        {QStringLiteral("name"), ChannelListModel::displayName(channel)},
        {QStringLiteral("topic"), qstr(channel.m_topic)},
        {QStringLiteral("nsfw"), channel.m_bNSFW},
        {QStringLiteral("iconUrl"), ChannelListModel::iconUrl(channel)},
    };
    const Channel* inCategory = &channel;
    if (channel.IsThread()) {
        // The channel the thread is in, and that channel's category.
        inCategory = channel.m_parentCateg ? instance.GetChannel(channel.m_parentCateg) : nullptr;
        if (inCategory)
            info.insert(QStringLiteral("channel"), QLatin1Char('#') + qstr(inCategory->m_name));
    }
    if (Channel* category = inCategory && inCategory->m_parentCateg ? instance.GetChannel(inCategory->m_parentCateg) : nullptr)
        info.insert(QStringLiteral("category"), qstr(category->m_name));
    if (Guild* guild = channel.m_parentGuild ? instance.GetGuild(channel.m_parentGuild) : nullptr)
        info.insert(QStringLiteral("server"), qstr(guild->m_name));
    if (channel.IsDM())
        info.insert(QStringLiteral("members"), members(instance, channel));
    return info;
}
