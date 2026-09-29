#include "ChannelListModel.h"

#include <algorithm>

#include "discord/DiscordInstance.hpp"
#include "discord/models/Permissions.hpp"
#include "discord/state/ProfileCache.hpp"

#include "DiscordUrls.h"

namespace {

QString kindOf(const Channel& channel)
{
    switch (channel.m_channelType) {
    case Channel::DM:         return QStringLiteral("dm");
    case Channel::GROUPDM:    return QStringLiteral("group");
    case Channel::CATEGORY:   return QStringLiteral("category");
    case Channel::NEWS:       return QStringLiteral("announcement");
    case Channel::VOICE:
    case Channel::STAGEVOICE: return QStringLiteral("voice");
    case Channel::FORUM:
    case Channel::MEDIA:      return QStringLiteral("forum");
    default:                  return QStringLiteral("text");
    }
}

// Voice channels have a text chat too; forums need thread support first.
bool isOpenable(const Channel& channel)
{
    switch (channel.m_channelType) {
    case Channel::CATEGORY:
    case Channel::FORUM:
    case Channel::MEDIA:
    case Channel::DIRECTORY:
    case Channel::STORE:
        return false;
    default:
        return true;
    }
}

bool isThread(const Channel& channel)
{
    const int type = channel.m_channelType;
    return type == Channel::NEWSTHREAD || type == Channel::PUBTHREAD || type == Channel::PRIVTHREAD;
}

bool byPosition(const Channel* a, const Channel* b)
{
    // Text-like channels before voice, then Discord's position order.
    if (a->IsVoice() != b->IsVoice())
        return !a->IsVoice();
    if (a->m_pos != b->m_pos)
        return a->m_pos < b->m_pos;
    return a->m_snowflake < b->m_snowflake;
}

}

QString ChannelListModel::displayName(const Channel& channel)
{
    QString name = QString::fromStdString(channel.m_name);
    if (!name.isEmpty() || channel.m_channelType != Channel::DM)
        return name;
    Profile* profile = GetProfileCache()->LookupProfile(channel.GetDMRecipient(), "", "", "", false);
    if (profile)
        name = QString::fromStdString(!profile->m_globalName.empty() ? profile->m_globalName : profile->m_name);
    return name;
}

int ChannelListModel::rowCount(const QModelIndex& parent) const
{
    return parent.isValid() ? 0 : int(m_ids.size());
}

QVariant ChannelListModel::data(const QModelIndex& index, int role) const
{
    DiscordInstance* instance = GetDiscordInstance();
    if (!instance || !index.isValid() || index.row() >= int(m_ids.size()))
        return QVariant();

    Guild* guild = instance->GetGuild(m_guild);
    Channel* channel = guild ? guild->GetChannel(m_ids[size_t(index.row())]) : nullptr;
    if (!channel)
        return QVariant();

    switch (role) {
    case ChannelIdRole:  return DiscordUrls::id(channel->m_snowflake);
    case NameRole:       return displayName(*channel);
    case KindRole:       return kindOf(*channel);
    case IsCategoryRole: return channel->IsCategory();
    case OpenableRole:   return isOpenable(*channel);
    case UnreadRole:     return channel->HasUnreadMessages();
    case MentionsRole:   return channel->m_mentionCount;
    case TopicRole:      return QString::fromStdString(channel->m_topic);
    case IndentedRole:
        return !channel->IsCategory() && !channel->IsDM()
               && channel->m_parentCateg != 0 && channel->m_parentCateg != channel->m_snowflake;
    case IconUrlRole:
        if (channel->m_channelType == Channel::DM)
            return DiscordUrls::userAvatar(channel->GetDMRecipient(), channel->m_avatarLnk);
        if (channel->m_channelType == Channel::GROUPDM)
            return DiscordUrls::channelIcon(channel->m_snowflake, channel->m_avatarLnk);
        return QString();
    }
    return QVariant();
}

QHash<int, QByteArray> ChannelListModel::roleNames() const
{
    return {
        {ChannelIdRole, "channelId"},
        {NameRole, "name"},
        {KindRole, "kind"},
        {IsCategoryRole, "isCategory"},
        {OpenableRole, "openable"},
        {UnreadRole, "unread"},
        {MentionsRole, "mentions"},
        {IconUrlRole, "iconUrl"},
        {TopicRole, "topic"},
        {IndentedRole, "indented"},
    };
}

void ChannelListModel::reload()
{
    beginResetModel();
    m_ids.clear();

    DiscordInstance* instance = GetDiscordInstance();
    Guild* guild = instance ? instance->GetCurrentGuild() : nullptr;
    m_guild = guild ? guild->m_snowflake : 0;

    if (guild && m_guild == 0) {
        // Direct messages: most recent conversation first.
        std::vector<const Channel*> dms;
        for (const Channel& channel : guild->m_channels) {
            if (channel.IsDM())
                dms.push_back(&channel);
        }
        std::sort(dms.begin(), dms.end(), [](const Channel* a, const Channel* b) {
            return a->m_lastSentMsg > b->m_lastSentMsg;
        });
        for (const Channel* channel : dms)
            m_ids.push_back(channel->m_snowflake);
    } else if (guild) {
        std::vector<Channel*> categories;
        std::vector<Channel*> children;
        for (Channel& channel : guild->m_channels) {
            if (isThread(channel) || !channel.HasPermission(PERM_VIEW_CHANNEL))
                continue;
            if (channel.IsCategory())
                categories.push_back(&channel);
            else
                children.push_back(&channel);
        }
        std::sort(categories.begin(), categories.end(), byPosition);
        std::sort(children.begin(), children.end(), byPosition);

        // Channels outside any category come first, then each category with
        // its channels. Empty categories are left out.
        auto isUncategorized = [&](const Channel* channel) {
            return std::none_of(categories.begin(), categories.end(), [&](const Channel* category) {
                return category->m_snowflake == channel->m_parentCateg;
            });
        };
        for (const Channel* channel : children) {
            if (isUncategorized(channel))
                m_ids.push_back(channel->m_snowflake);
        }
        for (const Channel* category : categories) {
            bool headerAdded = false;
            for (const Channel* channel : children) {
                if (channel->m_parentCateg != category->m_snowflake)
                    continue;
                if (!headerAdded) {
                    m_ids.push_back(category->m_snowflake);
                    headerAdded = true;
                }
                m_ids.push_back(channel->m_snowflake);
            }
        }
    }

    endResetModel();
    emit countChanged();
}

void ChannelListModel::refreshChannel(Snowflake channel)
{
    const auto it = std::find(m_ids.begin(), m_ids.end(), channel);
    if (it == m_ids.end())
        return;
    const QModelIndex row = index(int(it - m_ids.begin()));
    emit dataChanged(row, row);
}

void ChannelListModel::refreshAll()
{
    if (!m_ids.empty())
        emit dataChanged(index(0), index(int(m_ids.size()) - 1));
}

void ChannelListModel::clear()
{
    beginResetModel();
    m_ids.clear();
    m_guild = 0;
    endResetModel();
    emit countChanged();
}
