#pragma once

#include <set>

#include <QAbstractListModel>
#include <QHash>

#include <memory>
#include <vector>

#include "discord/models/Message.hpp"
#include "discord/models/Snowflake.hpp"

// Messages of the open channel, newest first (the chat ListView runs
// bottom-to-top). Rows are read from Discord Messenger's MessageCache; gap
// markers become the hasOlder flag instead of rows.
class MessageListModel : public QAbstractListModel
{
    Q_OBJECT
    Q_PROPERTY(int count READ rowCount NOTIFY countChanged)
    Q_PROPERTY(bool hasOlder READ hasOlder NOTIFY hasOlderChanged)
    Q_PROPERTY(bool reachedStart READ reachedStart NOTIFY hasOlderChanged)
    // Pixel sizes of custom emoji in messages, and in emoji-only messages
    // (up to three emoji, shown large). Set from QML in grid units.
    Q_PROPERTY(int emojiSize READ emojiSize WRITE setEmojiSize NOTIFY emojiSizeChanged)
    Q_PROPERTY(int jumboEmojiSize READ jumboEmojiSize WRITE setJumboEmojiSize NOTIFY emojiSizeChanged)

public:
    enum Roles {
        MessageIdRole = Qt::UserRole + 1,
        AuthorIdRole,
        AuthorRole,
        AvatarUrlRole,
        BodyRole,          // rich text
        PlainBodyRole,
        TimestampRole,     // short, e.g. "Today at 14:02"
        EditedRole,
        IsOwnRole,
        IsPendingRole,
        IsSystemRole,
        GroupedRole,       // same author as the message above, shortly before
        HasReplyRole,
        ReplyAuthorRole,
        ReplyBodyRole,
        MediaRole,         // attachments and media embeds, see mediaOf()
        ReactionsRole,     // list of {emoji, text, imageUrl, count, me}
        EmbedsRole,        // link previews and bot embeds, see embedsOf()
        SystemIconRole,    // Suru icon of a system message
        InteractionRole,   // "Alice used /ping" above a command's response
        ForwardedRole,     // the body is a forwarded message
        StickersRole,      // list of {name, url, animated, lottie}
        PollRole,          // see pollOf(), or null
        JumboRole,         // only 1-3 emoji: show them large
        SeparatedRole,     // not grouped with the message above: separator
        BlockedRole,       // from a user the account blocked
    };

    using QAbstractListModel::QAbstractListModel;

    int rowCount(const QModelIndex& parent = QModelIndex()) const override;
    QVariant data(const QModelIndex& index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;

    void setChannel(Snowflake guild, Snowflake channel);
    Snowflake channel() const { return m_channel; }
    Snowflake guild() const { return m_guild; }
    // Names changed (server nicknames arrived).
    void refreshNames();
    // Authors (and people replied to or mentioned) without a server member
    // profile here yet: their nicknames are not known.
    std::set<Snowflake> unknownMembers() const;

    // Re-reads the cache and applies the difference to the rows.
    void sync();
    void clear();

    bool hasOlder() const { return m_olderGap != 0; }
    bool reachedStart() const { return m_reachedStart; }
    // The GAP_UP marker to load older messages from, or null.
    MessagePtr olderGap() const;
    // Newest real message, for read acknowledgement.
    Snowflake newestMessageId() const;

    void setOwnUserId(Snowflake user) { m_ownUser = user; }

    int emojiSize() const { return m_emojiSize; }
    void setEmojiSize(int size);
    int jumboEmojiSize() const { return m_jumboEmojiSize; }
    void setJumboEmojiSize(int size);

signals:
    void countChanged();
    void hasOlderChanged();
    void emojiSizeChanged();

private:
    struct Row {
        MessagePtr message;
        bool grouped = false;
        bool separated = false; // not grouped with the message above: a separator
    };

    std::vector<Row> readCache(Snowflake& olderGap, bool& reachedStart) const;
    static void computeGrouping(std::vector<Row>& rows);
    QString richBody(const Message& message) const;
    static bool isJumbo(const Message& message);
    void refreshBodies();

    Snowflake m_guild = 0;
    Snowflake m_channel = 0;
    Snowflake m_ownUser = 0;
    Snowflake m_olderGap = 0;
    bool m_reachedStart = false;
    std::vector<Row> m_rows;
    int m_emojiSize = 20;
    int m_jumboEmojiSize = 48;
    mutable QHash<Snowflake, QString> m_bodyCache; // message id -> rich text
};
