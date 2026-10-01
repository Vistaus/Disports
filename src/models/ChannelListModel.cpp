#include "ChannelListModel.h"

#include <QDateTime>

#include <algorithm>

#include "discord/DiscordInstance.hpp"
#include "discord/models/Permissions.hpp"
#include "discord/state/ProfileCache.hpp"

#include "DiscordUrls.h"
#include "voice/VoiceStates.h"

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
    case Channel::NEWSTHREAD:
    case Channel::PUBTHREAD:
    case Channel::PRIVTHREAD: return QStringLiteral("thread");
    default:                  return QStringLiteral("text");
    }
}

// Voice channels have a text chat too. Forums are only lists of threads,
// which are listed under them.
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

// Threads we aren't in, listed per channel because they had messages
// lately.
const int MaxRecentThreads = 3;

// When the last message was sent (or the thread was made), from the
// snowflake.
qint64 lastActivityMs(const Channel& channel)
{
    const uint64_t id = std::max(channel.m_lastSentMsg, channel.m_snowflake);
    return qint64(id >> 22) + 1420070400000ll;
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

QString ChannelListModel::iconUrl(const Channel& channel)
{
    if (channel.m_channelType == Channel::DM)
        return DiscordUrls::userAvatar(channel.GetDMRecipient(), channel.m_avatarLnk);
    if (channel.m_channelType == Channel::GROUPDM)
        return DiscordUrls::channelIcon(channel.m_snowflake, channel.m_avatarLnk);
    return QString();
}

namespace {

QString statusOf(const Channel& channel)
{
    if (channel.m_channelType != Channel::DM)
        return QString();
    Profile* profile = GetProfileCache()->LookupProfile(channel.GetDMRecipient(), "", "", "", false);
    switch (profile ? profile->m_activeStatus : STATUS_OFFLINE) {
    case STATUS_ONLINE: return QStringLiteral("online");
    case STATUS_IDLE:   return QStringLiteral("idle");
    case STATUS_DND:    return QStringLiteral("dnd");
    default:            return QStringLiteral("offline");
    }
}

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
    case IndentedRole: {
        // Threads: when their channel is.
        const Channel* inCategory = channel;
        if (channel->IsThread() && guild)
            inCategory = guild->GetChannel(channel->m_parentCateg);
        return inCategory && !inCategory->IsCategory() && !inCategory->IsDM()
               && inCategory->m_parentCateg != 0 && inCategory->m_parentCateg != inCategory->m_snowflake;
    }
    case IconUrlRole:    return iconUrl(*channel);
    case StatusRole:     return statusOf(*channel);
    case BlockedRole: {
        DiscordInstance* instance = GetDiscordInstance();
        return channel->m_channelType == Channel::DM && instance && instance->IsUserBlocked(channel->GetDMRecipient());
    }
    case InCallRole:
        return m_voiceStates && (channel->IsDM() || channel->IsVoice()) && m_voiceStates->hasCall(channel->m_snowflake);
    case VoiceCountRole:
        return m_voiceStates && channel->IsVoice() ? int(m_voiceStates->usersIn(channel->m_snowflake).size()) : 0;
    case VoiceMembersRole: {
        QVariantList members;
        if (!m_voiceStates || !channel->IsVoice())
            return members;
        for (Snowflake id : m_voiceStates->usersIn(channel->m_snowflake)) {
            Profile* profile = GetProfileCache()->LookupProfile(id, "", "", "", false);
            QString name;
            if (profile) {
                auto member = profile->m_guildMembers.find(channel->m_parentGuild);
                name = QString::fromStdString(member != profile->m_guildMembers.end() && !member->second.m_nick.empty()
                    ? member->second.m_nick
                    : !profile->m_globalName.empty() ? profile->m_globalName : profile->m_name);
            }
            members.append(QVariantMap{
                {QStringLiteral("name"), name},
                {QStringLiteral("avatarUrl"), DiscordUrls::userAvatar(id, profile ? profile->m_avatarlnk : std::string())},
            });
            if (members.size() == 5)
                break;
        }
        return members;
    }
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
        {StatusRole, "status"},
        {BlockedRole, "blocked"},
        {InCallRole, "inCall"},
        {VoiceMembersRole, "voiceMembers"},
        {VoiceCountRole, "voiceCount"},
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
        std::vector<Channel*> threads;
        for (Channel& channel : guild->m_channels) {
            if (channel.IsThread()) {
                if (channel.HasPermission(PERM_VIEW_CHANNEL))
                    threads.push_back(&channel);
                continue;
            }
            // A category is listed when a channel in it can be seen, even if
            // the category itself cannot (as on Discord); see below.
            if (channel.IsCategory())
                categories.push_back(&channel);
            else if (channel.HasPermission(PERM_VIEW_CHANNEL))
                children.push_back(&channel);
        }
        std::sort(categories.begin(), categories.end(), byPosition);
        std::sort(children.begin(), children.end(), byPosition);
        // Under their channel, the most recently active first: the ones we
        // are in, the open one, and a few with messages in the last day.
        std::sort(threads.begin(), threads.end(), [](const Channel* a, const Channel* b) {
            return lastActivityMs(*a) > lastActivityMs(*b);
        });
        const qint64 dayAgo = QDateTime::currentMSecsSinceEpoch() - 24 * 3600 * 1000;
        const Snowflake open = instance->GetCurrentChannelID();
        auto addChannel = [&](const Channel* channel) {
            m_ids.push_back(channel->m_snowflake);
            int recent = 0;
            for (const Channel* thread : threads) {
                if (thread->m_parentCateg != channel->m_snowflake)
                    continue;
                const bool listed = thread->m_bJoined || thread->m_snowflake == open
                    || (lastActivityMs(*thread) > dayAgo && recent++ < MaxRecentThreads);
                if (listed)
                    m_ids.push_back(thread->m_snowflake);
            }
        };

        // Channels outside any category come first, then each category with
        // its channels. Empty categories are left out.
        auto isUncategorized = [&](const Channel* channel) {
            return std::none_of(categories.begin(), categories.end(), [&](const Channel* category) {
                return category->m_snowflake == channel->m_parentCateg;
            });
        };
        for (const Channel* channel : children) {
            if (isUncategorized(channel))
                addChannel(channel);
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
                addChannel(channel);
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
