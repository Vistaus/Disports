#include "UnreadDmListModel.h"

#include <algorithm>

#include "discord/DiscordInstance.hpp"

#include "DiscordUrls.h"
#include "ChannelListModel.h"

int UnreadDmListModel::rowCount(const QModelIndex& parent) const
{
    return parent.isValid() ? 0 : int(m_ids.size());
}

QVariant UnreadDmListModel::data(const QModelIndex& index, int role) const
{
    DiscordInstance* instance = GetDiscordInstance();
    if (!instance || !index.isValid() || index.row() >= int(m_ids.size()))
        return QVariant();
    Channel* channel = instance->m_dmGuild.GetChannel(m_ids[size_t(index.row())]);
    if (!channel)
        return QVariant();

    switch (role) {
    case ChannelIdRole: return DiscordUrls::id(channel->m_snowflake);
    case NameRole:      return ChannelListModel::displayName(*channel);
    case IconUrlRole:   return ChannelListModel::iconUrl(*channel);
    case InitialsRole:  return DiscordUrls::initials(ChannelListModel::displayName(*channel));
    case MentionsRole:  return channel->m_mentionCount;
    }
    return QVariant();
}

QHash<int, QByteArray> UnreadDmListModel::roleNames() const
{
    return {
        {ChannelIdRole, "channelId"},
        {NameRole, "name"},
        {IconUrlRole, "iconUrl"},
        {InitialsRole, "initials"},
        {MentionsRole, "mentions"},
    };
}

void UnreadDmListModel::reload()
{
    std::vector<const Channel*> unread;
    if (DiscordInstance* instance = GetDiscordInstance()) {
        for (const Channel& channel : instance->m_dmGuild.m_channels) {
            if (channel.IsDM() && channel.m_mentionCount > 0)
                unread.push_back(&channel);
        }
    }
    std::sort(unread.begin(), unread.end(), [](const Channel* a, const Channel* b) {
        return a->m_lastSentMsg > b->m_lastSentMsg;
    });

    std::vector<Snowflake> ids;
    for (const Channel* channel : unread)
        ids.push_back(channel->m_snowflake);
    if (ids == m_ids) {
        if (!m_ids.empty())
            emit dataChanged(index(0), index(int(m_ids.size()) - 1), {MentionsRole});
        return;
    }

    beginResetModel();
    m_ids = std::move(ids);
    endResetModel();
    emit countChanged();
}

void UnreadDmListModel::clear()
{
    beginResetModel();
    m_ids.clear();
    endResetModel();
    emit countChanged();
}
