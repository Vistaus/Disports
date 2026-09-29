#include "GuildListModel.h"

#include "discord/DiscordInstance.hpp"

#include "DiscordUrls.h"

namespace {

int guildMentions(Guild* guild)
{
    int mentions = 0;
    for (const Channel& channel : guild->m_channels)
        mentions += channel.m_mentionCount;
    return mentions;
}

}

int GuildListModel::rowCount(const QModelIndex& parent) const
{
    return parent.isValid() ? 0 : int(m_ids.size());
}

QVariant GuildListModel::data(const QModelIndex& index, int role) const
{
    DiscordInstance* instance = GetDiscordInstance();
    if (!instance || !index.isValid() || index.row() >= int(m_ids.size()))
        return QVariant();

    const Snowflake id = m_ids[size_t(index.row())];
    Guild* guild = instance->GetGuild(id);
    if (!guild)
        return QVariant();

    const QString name = QString::fromStdString(guild->m_name);
    switch (role) {
    case GuildIdRole:  return DiscordUrls::id(id);
    case NameRole:     return name;
    case IconUrlRole:  return DiscordUrls::guildIcon(id, guild->m_avatarlnk);
    case InitialsRole: return DiscordUrls::initials(name);
    case UnreadRole:   return guild->IsUnread();
    case MentionsRole: return guildMentions(guild);
    }
    return QVariant();
}

QHash<int, QByteArray> GuildListModel::roleNames() const
{
    return {
        {GuildIdRole, "guildId"},
        {NameRole, "name"},
        {IconUrlRole, "iconUrl"},
        {InitialsRole, "initials"},
        {UnreadRole, "unread"},
        {MentionsRole, "mentions"},
    };
}

void GuildListModel::reload()
{
    beginResetModel();
    m_ids.clear();
    if (DiscordInstance* instance = GetDiscordInstance()) {
        std::vector<Snowflake> ordered;
        instance->GetGuildIDsOrdered(ordered, false);
        for (Snowflake id : ordered) {
            if (id & BIT_FOLDER)
                continue;
            if (instance->GetGuild(id))
                m_ids.push_back(id);
        }
        // Guilds missing from the folder list (e.g. just joined) go last.
        for (const Guild& guild : instance->m_guilds) {
            if (std::find(m_ids.begin(), m_ids.end(), guild.m_snowflake) == m_ids.end())
                m_ids.push_back(guild.m_snowflake);
        }
    }
    endResetModel();
    emit countChanged();
    emit unreadChanged();
}

void GuildListModel::refreshUnread()
{
    if (!m_ids.empty())
        emit dataChanged(index(0), index(int(m_ids.size()) - 1), {UnreadRole, MentionsRole});
    emit unreadChanged();
}

void GuildListModel::clear()
{
    beginResetModel();
    m_ids.clear();
    endResetModel();
    emit countChanged();
    emit unreadChanged();
}

int GuildListModel::directMessageMentions() const
{
    DiscordInstance* instance = GetDiscordInstance();
    if (!instance)
        return 0;
    return guildMentions(&instance->m_dmGuild);
}
