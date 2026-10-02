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

QString statusName(eActiveStatus status)
{
    switch (status) {
    case STATUS_ONLINE: return QStringLiteral("online");
    case STATUS_IDLE:   return QStringLiteral("idle");
    case STATUS_DND:    return QStringLiteral("dnd");
    default:            return QStringLiteral("offline");
    }
}

}

QVariantMap describeUser(DiscordInstance& instance, Snowflake id)
{
    Profile* profile = GetProfileCache()->LookupProfile(id, "", "", "", false);
    if (!profile)
        return QVariantMap();
    const QString username = qstr(profile->m_name);
    return QVariantMap{
        {QStringLiteral("id"), DiscordUrls::id(id)},
        {QStringLiteral("name"), profile->m_globalName.empty() ? username : qstr(profile->m_globalName)},
        {QStringLiteral("self"), id == instance.GetUserID()},
        {QStringLiteral("username"), qstr(profile->m_name)},
        {QStringLiteral("avatarUrl"), DiscordUrls::userAvatar(id, profile->m_avatarlnk, 256)},
        {QStringLiteral("status"), statusName(profile->m_activeStatus)},
        {QStringLiteral("customStatus"), qstr(profile->m_status)},
        {QStringLiteral("bio"), qstr(profile->m_bio)},
        {QStringLiteral("pronouns"), qstr(profile->m_pronouns)},
        {QStringLiteral("bot"), profile->m_bIsBot},
        {QStringLiteral("blocked"), instance.IsUserBlocked(id)},
        {QStringLiteral("profileLoaded"), profile->m_bExtraDataFetched},
    };
}

namespace {

QVariantList members(DiscordInstance& instance, const Channel& channel)
{
    QVariantList list;
    // Discord lists the others; we are in the group too, first.
    std::vector<Snowflake> people{instance.GetUserID()};
    for (Snowflake id : channel.m_recipients) {
        if (id != instance.GetUserID())
            people.push_back(id);
    }
    for (Snowflake id : people) {
        Profile* profile = GetProfileCache()->LookupProfile(id, "", "", "", false);
        const QString username = profile ? qstr(profile->m_name) : QString();
        const QString name = profile && !profile->m_globalName.empty() ? qstr(profile->m_globalName) : username;
        list.append(QVariantMap{
            {QStringLiteral("id"), DiscordUrls::id(id)},
            {QStringLiteral("name"), name},
            {QStringLiteral("username"), username},
            {QStringLiteral("avatarUrl"), DiscordUrls::userAvatar(id, profile ? profile->m_avatarlnk : std::string())},
            {QStringLiteral("blocked"), instance.IsUserBlocked(id)},
            {QStringLiteral("self"), id == instance.GetUserID()},
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
    if (channel.m_channelType == Channel::DM && !channel.m_recipients.empty())
        info.insert(QStringLiteral("user"), describeUser(instance, channel.GetDMRecipient()));
    else if (channel.IsDM())
        info.insert(QStringLiteral("members"), members(instance, channel));
    return info;
}
