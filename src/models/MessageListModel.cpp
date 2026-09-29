#include "MessageListModel.h"

#include <QVariantMap>

#include <list>

#include "discord/state/MessageCache.hpp"

#include "DiscordUrls.h"
#include "MessageFormatter.h"

namespace {

// Messages from the same author within this window share one header.
constexpr time_t GroupWindowSeconds = 7 * 60;

bool isSystem(const Message& message)
{
    return !MessageFormatter::systemText(message).isEmpty();
}

bool sameRows(const std::vector<MessagePtr>& a, size_t aFrom,
              const std::vector<MessagePtr>& b, size_t bFrom, size_t count)
{
    for (size_t i = 0; i < count; ++i) {
        if (a[aFrom + i]->m_snowflake != b[bFrom + i]->m_snowflake)
            return false;
    }
    return true;
}

}

int MessageListModel::rowCount(const QModelIndex& parent) const
{
    return parent.isValid() ? 0 : int(m_rows.size());
}

QVariant MessageListModel::data(const QModelIndex& index, int role) const
{
    if (!index.isValid() || index.row() >= int(m_rows.size()))
        return QVariant();

    const Row& row = m_rows[size_t(index.row())];
    const Message& m = *row.message;
    const QString systemText = MessageFormatter::systemText(m);

    switch (role) {
    case MessageIdRole: return DiscordUrls::id(m.m_snowflake);
    case AuthorIdRole:  return DiscordUrls::id(m.m_author_snowflake);
    case AuthorRole:    return QString::fromStdString(m.m_author);
    case AvatarUrlRole: return DiscordUrls::userAvatar(m.m_author_snowflake, m.m_avatar);
    case BodyRole:
        return systemText.isEmpty() ? richBody(m) : systemText;
    case PlainBodyRole:
        return systemText.isEmpty() ? QString::fromStdString(m.m_message) : systemText;
    case TimestampRole:
        return m.m_type == MessageType::SENDING_MESSAGE ? QStringLiteral("Sending…")
                                                        : QString::fromStdString(m.m_dateCompact);
    case EditedRole:    return m.m_timeEdited != 0;
    case IsOwnRole:     return m_ownUser != 0 && m.m_author_snowflake == m_ownUser;
    case IsPendingRole: return m.m_type == MessageType::SENDING_MESSAGE;
    case IsSystemRole:  return !systemText.isEmpty();
    case GroupedRole:   return row.grouped;
    case HasReplyRole:  return m.IsReply();
    case ReplyAuthorRole:
        return m.m_pReferencedMessage ? QString::fromStdString(m.m_pReferencedMessage->m_author) : QString();
    case ReplyBodyRole:
        return m.m_pReferencedMessage
                   ? MessageFormatter::plainText(QString::fromStdString(m.m_pReferencedMessage->m_message), m_guild)
                   : QString();
    case AttachmentsRole: {
        QVariantList list;
        for (const Attachment& attachment : m.m_attachments) {
            const std::string& url = attachment.m_proxyUrl.empty() ? attachment.m_actualUrl : attachment.m_proxyUrl;
            list.append(QVariantMap{
                {QStringLiteral("url"), QString::fromStdString(url)},
                {QStringLiteral("fileName"), QString::fromStdString(attachment.m_fileName)},
                {QStringLiteral("isImage"), attachment.IsImage()},
                {QStringLiteral("width"), attachment.m_width},
                {QStringLiteral("height"), attachment.m_height},
            });
        }
        return list;
    }
    }
    return QVariant();
}

QHash<int, QByteArray> MessageListModel::roleNames() const
{
    return {
        {MessageIdRole, "messageId"},
        {AuthorIdRole, "authorId"},
        {AuthorRole, "author"},
        {AvatarUrlRole, "avatarUrl"},
        {BodyRole, "body"},
        {PlainBodyRole, "plainBody"},
        {TimestampRole, "timestamp"},
        {EditedRole, "edited"},
        {IsOwnRole, "isOwn"},
        {IsPendingRole, "isPending"},
        {IsSystemRole, "isSystem"},
        {GroupedRole, "grouped"},
        {HasReplyRole, "hasReply"},
        {ReplyAuthorRole, "replyAuthor"},
        {ReplyBodyRole, "replyBody"},
        {AttachmentsRole, "attachments"},
    };
}

void MessageListModel::setChannel(Snowflake guild, Snowflake channel)
{
    if (m_channel == channel && m_guild == guild)
        return;
    m_guild = guild;
    m_channel = channel;
    clear();
    sync();
}

void MessageListModel::clear()
{
    beginResetModel();
    m_rows.clear();
    m_bodyCache.clear();
    endResetModel();
    m_olderGap = 0;
    m_reachedStart = false;
    emit countChanged();
    emit hasOlderChanged();
}

std::vector<MessageListModel::Row> MessageListModel::readCache(Snowflake& olderGap, bool& reachedStart) const
{
    olderGap = 0;
    reachedStart = false;
    std::vector<Row> rows;
    if (!m_channel)
        return rows;

    std::list<MessagePtr> cached;
    GetMessageCache()->GetLoadedMessages(m_channel, m_guild, cached);

    // The cache is ordered oldest first; rows are newest first.
    for (auto it = cached.rbegin(); it != cached.rend(); ++it) {
        const MessagePtr& message = *it;
        if (message->m_type == MessageType::GAP_UP) {
            // Keep the oldest gap: that is where older history continues.
            olderGap = message->m_snowflake;
            continue;
        }
        if (message->m_type == MessageType::CHANNEL_HEADER) {
            reachedStart = true;
            continue;
        }
        if (message->IsLoadGap())
            continue;
        rows.push_back(Row{message, false});
    }
    computeGrouping(rows);
    return rows;
}

void MessageListModel::computeGrouping(std::vector<Row>& rows)
{
    for (size_t i = 0; i < rows.size(); ++i) {
        rows[i].grouped = false;
        if (i + 1 >= rows.size())
            continue;
        const Message& current = *rows[i].message;
        const Message& older = *rows[i + 1].message;
        if (current.m_author_snowflake != older.m_author_snowflake)
            continue;
        if (current.IsReply() || isSystem(current) || isSystem(older))
            continue;
        if (current.m_dateTime - older.m_dateTime > GroupWindowSeconds)
            continue;
        rows[i].grouped = true;
    }
}

void MessageListModel::sync()
{
    Snowflake olderGap = 0;
    bool reachedStart = false;
    std::vector<Row> fresh = readCache(olderGap, reachedStart);

    std::vector<MessagePtr> oldIds, newIds;
    for (const Row& row : m_rows)
        oldIds.push_back(row.message);
    for (const Row& row : fresh)
        newIds.push_back(row.message);

    const size_t oldCount = oldIds.size();
    const size_t newCount = newIds.size();

    // Work out where the old rows sit inside the new list, so that new
    // messages (front) and older history (back) are inserted instead of
    // resetting the view.
    size_t front = 0;
    bool aligned = false;
    if (oldCount > 0 && newCount >= oldCount) {
        for (size_t offset = 0; offset + oldCount <= newCount; ++offset) {
            if (newIds[offset]->m_snowflake == oldIds[0]->m_snowflake) {
                if (sameRows(newIds, offset, oldIds, 0, oldCount)) {
                    front = offset;
                    aligned = true;
                }
                break;
            }
        }
    }

    // Only deletions: the new list is the old one with some rows missing.
    bool onlyRemovals = false;
    if (!aligned && oldCount > 0 && newCount < oldCount) {
        size_t j = 0;
        for (size_t i = 0; i < oldCount && j < newCount; ++i) {
            if (oldIds[i]->m_snowflake == newIds[j]->m_snowflake)
                ++j;
        }
        onlyRemovals = j == newCount;
    }

    if (onlyRemovals) {
        size_t j = 0;
        for (size_t i = 0; i < m_rows.size();) {
            if (j < fresh.size() && m_rows[i].message->m_snowflake == fresh[j].message->m_snowflake) {
                m_rows[i] = fresh[j];
                ++i;
                ++j;
                continue;
            }
            beginRemoveRows(QModelIndex(), int(i), int(i));
            m_bodyCache.remove(m_rows[i].message->m_snowflake);
            m_rows.erase(m_rows.begin() + long(i));
            endRemoveRows();
        }
        if (!m_rows.empty())
            emit dataChanged(index(0), index(int(m_rows.size()) - 1), {GroupedRole});
        emit countChanged();
    } else if (!aligned) {
        beginResetModel();
        m_rows = std::move(fresh);
        endResetModel();
        emit countChanged();
    } else {
        const size_t back = newCount - oldCount - front;
        if (front > 0) {
            beginInsertRows(QModelIndex(), 0, int(front) - 1);
            m_rows.insert(m_rows.begin(), fresh.begin(), fresh.begin() + long(front));
            endInsertRows();
        }
        if (back > 0) {
            const int first = int(m_rows.size());
            beginInsertRows(QModelIndex(), first, first + int(back) - 1);
            m_rows.insert(m_rows.end(), fresh.end() - long(back), fresh.end());
            endInsertRows();
        }
        // Content (edits, embeds, grouping) may have changed for existing
        // rows; message objects are replaced on edit, so refresh them all.
        for (size_t i = 0; i < m_rows.size(); ++i) {
            if (m_rows[i].message != fresh[i].message)
                m_bodyCache.remove(fresh[i].message->m_snowflake);
            m_rows[i] = fresh[i];
        }
        if (!m_rows.empty())
            emit dataChanged(index(0), index(int(m_rows.size()) - 1));
        if (front > 0 || back > 0)
            emit countChanged();
    }

    if (olderGap != m_olderGap || reachedStart != m_reachedStart) {
        m_olderGap = olderGap;
        m_reachedStart = reachedStart;
        emit hasOlderChanged();
    }
}

MessagePtr MessageListModel::olderGap() const
{
    if (!m_olderGap || !m_channel)
        return nullptr;
    return GetMessageCache()->GetLoadedMessage(m_channel, m_olderGap);
}

Snowflake MessageListModel::newestMessageId() const
{
    for (const Row& row : m_rows) {
        if (row.message->m_type != MessageType::SENDING_MESSAGE)
            return row.message->m_snowflake;
    }
    return 0;
}

QString MessageListModel::richBody(const Message& message) const
{
    auto it = m_bodyCache.constFind(message.m_snowflake);
    if (it != m_bodyCache.constEnd())
        return *it;
    const QString body = MessageFormatter::richText(QString::fromStdString(message.m_message), m_guild);
    m_bodyCache.insert(message.m_snowflake, body);
    return body;
}
